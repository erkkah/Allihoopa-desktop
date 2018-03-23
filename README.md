Allihoopa Desktop SDK
=====================

_Technology preview_

The Allihoopa Desktop SDK provides an interface to the [Allihoopa][] music collaboration service for apps running on MacOS and Windows.

The SDK is a thin `C` interface provided in source form, and has no external dependencies except for what is provided by the C standard library.

The SDK connects to a locally installed Allihoopa App that provides all user interaction and service communication.
This effectively decouples the host app from that complexity, and lets the host app and Allihoopa App have separate update cycles.

## Installation

The API is exposed in the `allihoopa.h` header, and the implementation is in `allihoopa.c`. Simply copy these two into your project and make them compile and link as usual.

>Setting the DEBUG define to a non-zero value will enable some level of tracing to stderr.

## API concepts

To keep the API free of dependencies, and support a wide range of implementation environments, we have set on a couple of simple concepts.

*   **All API calls return an error code**

    Calls will either succeed or fail with an error code.
    A successful call means that a request was successfully sent to the Allihoopa App, and that a response is to be expected.
    Failed calls will not generate responses.

*   **API calls are asynchronous**

    Calls to the SDK will respond quickly, even if the actual request takes a while to execute.
    If the caller is interested in the response, it can be fetched using a separate call.

*   **Complex data is passed as UTF-8 - encoded JSON.**

    This is to avoid having to allocate and manage complex data structures in plain `C`.
    Complex data is always owned by the caller; if you allocated the memory, you free it. *This implies that data returned in callbacks is only valid during that callback, and should be copied if needed.*

*   **Blob files**

    Big binary data, such as audio and image files, are passed as URLs in the JSON data structures. The SDK accepts `http:`, `https:`, `data:` and `file:` protocols. Using the `file:` protocol refers to a client specified temporary directory, and is the recommended method. See the `AHinit` call.

*   **Requests can be tagged with unique IDs**

    API calls accept an integer request ID that is used to tag the request. This is used to match responses to requests, and can be used to free up client side resources associated with a request. The ID must be non-zero, but is not otherwise interpreted by the SDK. *Passing the same non-zero ID to all requests is OK.*

*   **Thread safety**

    The SDK is not thread safe, meaning that all API calls must be made from the same thread. Blocking IO requests between the library and the Allihoopa App will time out after 5 seconds and cause API calls to return `AHErrorCommsFailure`.

## Implementation flow

First, initialize the library by calling `AHinit` with your Allihoopa application ID and API key.

>Contact Allihoopa at developer@allihoopa.com to register your app and get your API key.

You can also provide the root directory for `file:` protocol URLs in the `AHinit` call by setting the `tmpDir` parameter. If not set, this defaults to the system temp directory.

If initialization succeeds, the Allihoopa App window appears, waiting for requests. If the initialization fails with error code `AHErrorAppNotFound`, the Allihoopa App is not installed, or the installation is somehow broken. In that case, direct the user to the `AHSDKHelpURL` url for guidance.

### Dropping

To initiate a minimal drop request, pass a JSON object like this to `AHdrop`:

```json
{
    "stems": {
        "mixStemURL": "file:///my_little_song.wav"
    },
    "presentation": {
        "title": "Singa singa song :)"
    },
    "musicalMetadata": {
        "lengthMicroseconds": 5217392
    }
}
```

If the call succeeds, it will return immediately, and the SDK will start uploading the audio and presenting a dialog for the user.

>NOTE: This is just a minimal example. Please refer to the [pre-release checklist](https://gist.github.com/ReMarkus/ec375c31277cc46cfcc026e69f67c01a) to verify that you’ve integrated our SDK correctly.


### Polling for results

To check the results of previous requests, or to free up temporary resources, call `AHpollCompletedRequests` with a callback function that will be called once for each completed request.

The callback will be called with a JSON object in the following format:

```json
{
    "requestID": 1234,
    "data": {
        ...
    }
}
```

### Queueing up requests and closing the App

Although asynchronous, the SDK will only handle one request at a time. It is OK to initiate a new request before previous requests are handled. The requests will be put in a queue and handled one at a time in order.

>The request and response queues are limited to 100 items each. If the request queue is full, the next request will block. If the reponse queue gets filled up, the oldest response is overwritten.

### More...

This is just a brief overview, see [allihoopa.h](allihoopa.h) for details.

## Allihoopa App installation

The Allihoopa App is the required for the SDK to function, however it is not included in the SDK distribution. The information at the `AHSDKHelpURL` provides information about Allihoopa and guides the end user in downloading and installing the Allihoopa App.

>It is important that the error code `AHErrorAppNotFound` is handled correctly by providing the end user with this information. Simply opening the URL in the system web browser is enough.

[Allihoopa]: https://allihoopa.com
