# LPBackend RESTful APIs

## User management

1. POST	/api/v1/auth/register
	- Request a register session
	- Request:
		```json
		{}
		```
	- Response:
		- on success:
		``` json
		{
			"success": true,
			"sessionId": "<register session id>",
			"captcha": {
				"data": "data:image/jpeg,base64,<data>",
				"expireInSeconds": "<seconds>"
			}
		}
		```

2. PUT /api/v1/auth/register/<session id>
