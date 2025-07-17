/*
 * Copyright (c) 2025 Laptis
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#pragma once

#include <list>
#include <mutex>

#include <boost/asio.hpp>

namespace lpbackend::asio
{
/** @brief A thread-safe task group that tracks child tasks, allows emitting
    cancellation signals to them, and waiting for their completion.
*/
class task_group
{
    std::mutex mutex_;
    boost::asio::steady_timer timer_;
    std::list<boost::asio::cancellation_signal> signals_;

  public:
    explicit inline task_group(const boost::asio::any_io_executor exec)
        : timer_{std::move(exec), boost::asio::steady_timer::time_point::max()}
    {
    }

    /** @brief Adds a cancellation slot and a wrapper object that will remove the child
        task from the list when it completes.

        @param completion_token The completion token that will be adapted.

        @par Thread Safety
        @e Distinct @e objects: Safe.@n
        @e Shared @e objects: Safe.
    */
    template <typename CompletionToken> inline auto adapt(CompletionToken &&completion_token)
    {
        std::lock_guard lock{mutex_};
        auto signal{signals_.emplace(signals_.end())};

        class remover
        {
            task_group *tg_;
            decltype(signal) signal_;

          public:
            remover(task_group *const tg, decltype(signal) signal) noexcept : tg_{tg}, signal_{signal}
            {
            }

            explicit remover(remover &&other) noexcept : tg_{std::exchange(other.tg_, nullptr)}, signal_{other.signal_}
            {
            }

            ~remover()
            {
                if (!tg_)
                {
                    return;
                }
                std::lock_guard lock{tg_->mutex_};
                if (tg_->signals_.erase(signal_) == tg_->signals_.end()) // if the list is empty
                {
                    // wake all `async_wait`s
                    tg_->timer_.cancel();
                }
            }
        };

        return boost::asio::bind_cancellation_slot(
            signal->slot(),
            boost::asio::consign(std::forward<CompletionToken>(completion_token), remover{this, signal}));
    }

    /** @brief Emits the signal to all child tasks and invokes the slot's
        handler, if any.

        @param type The completion type that will be emitted to child tasks.

        @par Thread Safety
        @e Distinct @e objects: Safe.@n
        @e Shared @e objects: Safe.
    */
    void emit(const boost::asio::cancellation_type type)
    {
        std::lock_guard lock{mutex_};
        for (auto &signal : signals_)
        {
            signal.emit(type);
        }
    }

    /** @brief Starts an asynchronous wait on the task_group.

        The completion handler will be called when:

        @li All the child tasks completed.
        @li The operation was cancelled.

        @param completion_token The completion token that will be used to
        produce a completion handler. The function signature of the completion
        handler must be:
        @code
        void handler(
            boost::system::error_code const& error  // result of operation
        );
        @endcode

        @par Thread Safety
        @e Distinct @e objects: Safe.@n
        @e Shared @e objects: Safe.
    */
    template <typename CompletionToken = boost::asio::default_completion_token_t<boost::asio::any_io_executor>>
    auto async_wait(
        CompletionToken &&completion_token = boost::asio::default_completion_token_t<boost::asio::any_io_executor>{})
    {
        return boost::asio::async_compose<CompletionToken, void(boost::system::error_code)>(
            [this, scheduled = false](auto &&self, boost::system::error_code ec = {}) mutable {
                if (!scheduled)
                {
                    self.reset_cancellation_state(boost::asio::enable_total_cancellation());
                }

                if (!self.cancelled() && ec == boost::asio::error::operation_aborted)
                {
                    ec = {};
                }

                {
                    std::lock_guard lock{mutex_};

                    if (!signals_.empty() && !ec)
                    {
                        scheduled = true;
                        return timer_.async_wait(std::move(self));
                    }
                }

                if (!std::exchange(scheduled, true))
                {
                    return boost::asio::post(boost::asio::append(std::move(self), ec));
                }

                self.complete(ec);
            },
            completion_token, timer_);
    }

    explicit task_group(const task_group &) = delete;
    explicit task_group(task_group &&) = delete;
};
} // namespace lpbackend::asio
