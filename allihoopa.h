/*

Allihoopa Desktop SDK
Copyright 2018 Allihoopa AB

*/

// All API methods accept a JSON formatted request and 
// return a non-zero error code on failure.

#ifndef ALLIHOOPA_H
#define ALLIHOOPA_H
#ifdef __cplusplus
extern "C" {
#endif

/*
    Prepares for Allihoopa communication.
    Must be called before any other API function.

    Setup data format:

    {
        "appID": "<appid>",
        "appKey": "<appkey>",
        "tmpDir": "[optional tempDir where file: - urls are referenced, defaults to system tempdir]"
    }
*/
int AHsetup(const char* setupData, short unsigned int setupDataLength);

/*
    Initiates a drop request

    dropData - utf-8 encoded json object
    requestID - client specific identifier for this request, must not be zero

    returns zero on success, non-zero error code on failure
*/
int AHdrop(const char* dropData, short unsigned int dropDataLength, short int requestID);

/*
    Closes the Allihoopa app.
    Further requests will open a new instance of the app.

    returns zero on success, non-zero error code on failure
*/
int AHclose();

/*
    Callback for receiving results from AHpollCompletedRequests.

    Each response will be json encoded object in the following format:

    {
        "requestID": 1234,
        "data": {
            ...
        }
    }

    responseData: utf-8 encoded json object. Only valid during the duration of the callback.
    responseLength: Length of the responseData in bytes.
*/
typedef void (*AHCompletionHandler)(const char* responseData, unsigned short int responseLength);

/*
    Polls for completed requests.
    Will call the specified handler for each completed request.
*/
int AHpollCompletedRequests(AHCompletionHandler handler);

/*
    Converts from an AHErrors error code to a printable
    string, for logging and debugging.
*/
const char* AHerrorCodeToMessage(int errorCode);

enum AHErrors {
    AHErrorAppNotFound = 451,
    AHErrorLaunchFailure,
    AHErrorCommsFailure,
    AHErrorInvalidRequest,
    AHErrorOutOfMemory,
    AHRequestFailed,
    AHErrorUnknownError,
};

#define AHMaxRequestBody 65535
#define AHSDKHelpURL "https://allihoopa.com/partnerapphelp"

/*

Drop request data, minimal:

{
    "stems": {
        "mixStem": "file:///url/to/audio/data"
    },
    "presentation": {
        "title": "Piece Title"
    },
    "musicalMetadata": {
        "lengthMicroseconds": 123456
    }
}

Drop request data, full format:

{
    "stems": {
        "mixStemURL": "file:///url/to/audio/data"
    },
    "presentation": {
        "title": "Piece Title",
        "previewURL": "file:///optional/url/to/preview/data",
        "coverImageURL": "file:///optional/url/to/cover/image"
    },
    "attribution": {
        "basedOnPieces": [
            "pieceID_A", "pieceID_B"
        ]
    },
    "musicalMetadata": {
        "lengthMicroseconds": 123456,
        "tempo": {
            "fixed": 120.0
        },
        "loop": {
            "startMicroSeconds": 87,
            "endMicroSeconds": 19239
        },
        "timeSignature": {
            "fixed": {
                "upper": 2,
                "lower": 4
            }
        },
        "tonality": {
            "mode": "TONAL",
            "scale": [
                true, false,  true, false,  true,  true, false,  true, false,  true, false,  true
            ],
            "root": 0
        }
    },
    "attachments": [
        {
            "mimeType": "application/x-weird",
            "dataURL": "file:///url/to/attachment/data"
        }
    ]
}

*/

#ifdef __cplusplus
}
#endif

#endif // ALLIHOOPA_H
