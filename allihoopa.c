/*

Allihoopa Cross-platform SDK
Copyright 2018 Allihoopa AB

*/


/*

Client code - launches Allihoopa app and communicates using pipes.

Simple IPC used in pipes is based on fixed length messages to
simplify parsing code while being somewhat extensible.

Header:
    4 byte request code ('drop', et.c.)
    2 byte request ID (caller specific, non-zero identifier for the request)
    2 byte unsigned little endian body length (max 65535 bytes)

Body:
    <length> bytes json encoded object

*/

#include "allihoopa.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#if DEBUG
#define TRACE(msg) fprintf(stderr, "%s\n", msg)
#define TRACEF(...) fprintf(stderr, __VA_ARGS__)
#else
#define TRACE(msg)
#define TRACEF(...)
#endif

// Platform specifics with different implementations below
static int readFromApp(char* data, size_t length);
static int writeToApp(const char* data, size_t length);

// Local cross platform glue
static int callApp(
    short int requestID,
    const char* command,
    const char* data, size_t dataLength,
    const char** oBody, size_t* oBodyLength
);

// Exported functions

int AHsetup(const char* setupData, short unsigned int setupDataLength) {
    if (setupData == NULL || setupDataLength == 0) {
        return AHErrorInvalidRequest;
    }
    return callApp(0, "init", setupData, setupDataLength, 0, 0);
}

int AHdrop(const char* dropData, short unsigned int dropDataLength, short int requestID) {
    if (dropData == NULL || dropDataLength == 0 || requestID == 0) {
        return AHErrorInvalidRequest;
    }
    return callApp(requestID, "drop", dropData, dropDataLength, 0, 0);
}

int AHclose() {
    // Since we are closing down the app, ignore but return failed quit requests
    int result = callApp(0, "quit", 0, 0, 0, 0);
    return result;
}

int AHpollCompletedRequests(AHCompletionHandler handler) {
    int moreResults = 0;

    do {
        const char* body = 0;
        size_t bodyLength = 0;

        int pollResult = callApp(0, "poll", 0, 0, &body, &bodyLength);
        if(pollResult == 0) {
            if(bodyLength > AHMaxRequestBody) {
                return AHErrorCommsFailure;
            }
            if(bodyLength != 0) {
                handler(body, bodyLength);
                moreResults = 1;
            }
            else {
                moreResults = 0;
            }
        }
        else {
            return pollResult;
        }
    } while(moreResults);

    return 0;
}

const char* AHerrorCodeToMessage(int errorCode) {
    switch(errorCode) {
        case AHErrorCommsFailure:
            return "App communication failure";
        case AHErrorInvalidRequest:
            return "Invalid request";
        case AHErrorLaunchFailure:
            return "App launch failure";
        case AHErrorOutOfMemory:
            return "Out of memory";
        case AHErrorUnknownError:
        default:
            return "Unknown error";
    }
}

static int callApp(
    short int requestID,
    const char command[4],
    const char* data, size_t dataLength,
    const char** oBody, size_t* oBodyLength)
{
    if(command == 0){
        return AHErrorInvalidRequest;
    }

    if(data == 0 && dataLength != 0){
        return AHErrorInvalidRequest;
    }

    if(data != 0 && dataLength == 0){
        return AHErrorInvalidRequest;
    }

    if(dataLength > AHMaxRequestBody){
        return AHErrorInvalidRequest;
    }

    int result = writeToApp(command, 4);
    if (result) {
        return result;
    }
    result = writeToApp((char*) &requestID, 2);
    if (result) {
        return result;
    }
    result = writeToApp((char*) &dataLength, 2);
    if (result) {
        return result;
    }
    result = writeToApp(data, dataLength);
    if (result) {
        return result;
    }

    char reply[4] = {6, 6, 6, 6};
    result = readFromApp(&reply[0], 4);
    if (result) {
        return result;
    }

    short int responseID = 0;
    result = readFromApp((char*) &responseID, 2);
    if (result) {
        return result;
    }

    if (responseID != requestID) {
        TRACE("Request / response mismatch");
        return AHErrorCommsFailure;
    }

    short unsigned int replyBodyLength = 0;
    result = readFromApp((char*) &replyBodyLength, 2);
    if (result) {
        return result;
    }

    if(replyBodyLength > 0){
        char* body = malloc(replyBodyLength);
        if(body == 0){
            return AHErrorOutOfMemory;
        }
        int bodyResult = readFromApp(body, replyBodyLength);

        if (bodyResult != 0){
            free(body);
            return bodyResult;
        }

        // debug print body here ?

        if (oBody != 0) {
            *oBody = body;
            *oBodyLength = replyBodyLength;
        }
        else {
            free(body);
        }
    }

    static union {
        char chars[4];
        int code;
    } AOK = {
        {'o', 'k', 'a', 'y'}
    };

    TRACEF("Reply: %.4s, %X, %X\n", reply, *(int*) reply, AOK.code);
    if (*(int*) reply != AOK.code) {
        return AHRequestFailed;
    }

    // All OK!
    return 0;
}


/// Platform specific implementations

#ifdef _WIN32
#include <windows.h>

static HANDLE appProcessHandle = 0;
static HANDLE appInputWriteHandle = 0;
static HANDLE appInputReadHandle = 0;
static HANDLE appOutputWriteHandle = 0;
static HANDLE appOutputReadHandle = 0;

/*
 * Creates a pipe with overlapped read, since this is not possible
 * with the regular CreatePipe call.
 * This makes it possible to add timeouts to pipe read, avoiding
 * inter process deadlocks.
 */
static int createPipeWithOverlappedRead(
    HANDLE* oReadHandle,
    HANDLE* oWriteHandle,
    SECURITY_ATTRIBUTES* securityAttributes
)
{
    static int pipeSerial = 0;
    char pipeName[MAX_PATH];

    sprintf(pipeName, "\\\\.\\Pipe\\Allihoopa.%08x.%08x",
        GetCurrentProcessId(),
        pipeSerial++);
    
    const int numPipes = 1;
    const int bufferSize = 8192;
    const int defaultTimeoutMS = 100;

    *oReadHandle = CreateNamedPipe(
        pipeName,
        PIPE_ACCESS_INBOUND | FILE_FLAG_OVERLAPPED,
        PIPE_TYPE_BYTE | PIPE_WAIT,
        numPipes,
        bufferSize,
        bufferSize,
        defaultTimeoutMS,
        securityAttributes
    );

    if(!*oReadHandle){
        return 0;
    }

    *oWriteHandle = CreateFile(
        pipeName,
        GENERIC_WRITE,
        0, // not shared
        securityAttributes,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        NULL // no template file
    );

    if(*oWriteHandle == INVALID_HANDLE_VALUE) {
        CloseHandle(*oReadHandle);
        *oReadHandle = 0;
        *oWriteHandle = 0;
        return 0;
    }

    return 1;
}

static int initAppConnection() {
    TRACE("initAppConnection");
    if (appProcessHandle != 0) {
        DWORD exitCode = 0;
        if (GetExitCodeProcess(appProcessHandle, &exitCode) && exitCode == STILL_ACTIVE) {
            TRACE("initAppConnection: reusing live app");
            return 0;
        }
        
        TerminateProcess(appProcessHandle, 0);
        CloseHandle(appProcessHandle);
        appProcessHandle = 0;
    }

    SECURITY_ATTRIBUTES securityAttributes;
    securityAttributes.nLength = sizeof(SECURITY_ATTRIBUTES);
    securityAttributes.bInheritHandle = TRUE;
    securityAttributes.lpSecurityDescriptor = NULL;

    if (appInputReadHandle != 0) {
        CloseHandle(appInputReadHandle);
        appInputReadHandle = 0;
    }
    if (appInputWriteHandle != 0) {
        CloseHandle(appInputWriteHandle);
        appInputWriteHandle = 0;
    }
    if (appOutputReadHandle != 0) {
        CloseHandle(appOutputReadHandle);
        appOutputReadHandle = 0;
    }
    if (appOutputWriteHandle != 0) {
        CloseHandle(appOutputWriteHandle);
        appOutputWriteHandle = 0;
    }

    if(!CreatePipe(&appInputReadHandle, &appInputWriteHandle, &securityAttributes, 0)){
        TRACE("Failed to create app input pipe");
        return AHErrorLaunchFailure;
    }

    if(!SetHandleInformation(appInputWriteHandle, HANDLE_FLAG_INHERIT, 0)) {
        return AHErrorLaunchFailure;
    }

    if(!createPipeWithOverlappedRead(&appOutputReadHandle, &appOutputWriteHandle, &securityAttributes)){
        TRACE("Failed to create app output pipe");
        return AHErrorLaunchFailure;
    }

    if(!SetHandleInformation(appOutputReadHandle, HANDLE_FLAG_INHERIT, 0)) {
        return AHErrorLaunchFailure;
    }

    PROCESS_INFORMATION processInfo;
    ZeroMemory(&processInfo, sizeof(PROCESS_INFORMATION));

    STARTUPINFO startupInfo;
    ZeroMemory(&startupInfo, sizeof(STARTUPINFO));
    startupInfo.cb = sizeof(STARTUPINFO);
    startupInfo.hStdOutput = appOutputWriteHandle;
    // ??? Cannot get redirection to work, using direct console writes from App instead
    //startupInfo.hStdError = stdErrorHandle;
    startupInfo.hStdInput = appInputReadHandle;
    startupInfo.dwFlags |= STARTF_USESTDHANDLES;
    startupInfo.dwFlags |= STARTF_USESHOWWINDOW;
    startupInfo.wShowWindow = SW_SHOWDEFAULT;

    char* cmdLine = "allihoopa.exe -pipe";

    BOOL result = CreateProcess(
        NULL,       // app name
        cmdLine,    // commandline
        NULL,       // security attributes
        NULL,       // thread security attributes
        TRUE,       // inherit handles
        0,          // creation flags
        NULL,       // use parent environment
        NULL,       // use parent's cwd
        &startupInfo,
        &processInfo
        );
    
    if(!result) {
        return AHErrorLaunchFailure;
    }

    appProcessHandle = processInfo.hProcess;
    CloseHandle(processInfo.hThread);

    return 0;
}

static int writeToApp(const char* data, size_t length) {
    TRACE("writeToApp");
    {
        int result = initAppConnection();
        if (result != 0) {
            return result;
        }
    }
    {
        DWORD bytesWritten = 0;
        BOOL result = WriteFile(appInputWriteHandle, data, length, &bytesWritten, 0);
        if ((bytesWritten != length) || !result){
            return AHErrorCommsFailure;
        } else {
            return 0;
        }
    }
}

static int readFromApp(char* data, size_t length) {
    TRACE("readFromApp");
    {
        int result = initAppConnection();
        if (result != 0) {
            return result;
        }
    }
    {
        int ahResult = 0;
        OVERLAPPED overlapInfo;
        ZeroMemory(&overlapInfo, sizeof(OVERLAPPED));
        // We will wait for this event to get signalled on completion
        overlapInfo.hEvent = CreateEvent(
            NULL,
            TRUE, // manual reset
            FALSE, // initial state, not triggered
            NULL);

        if(overlapInfo.hEvent == NULL) {
            return AHErrorUnknownError;
        }

        BOOL readResult = ReadFile(appOutputReadHandle, data, length, 0, &overlapInfo);
        if (!readResult) {
            if (GetLastError() == ERROR_IO_PENDING) {
                DWORD bytesRead = 0;
                int timeoutMS = 1000 * 5;

                int waitResult = WaitForSingleObject(overlapInfo.hEvent, timeoutMS);
                if (waitResult != WAIT_OBJECT_0) {
                    ahResult = AHErrorCommsFailure;
                }
                else {
                    BOOL overlappedResult = GetOverlappedResult(
                        appOutputReadHandle,
                        &overlapInfo,
                        &bytesRead,
                        FALSE // don't block since we waited above
                    );
                    if (!overlappedResult) {
                        // Check GetLastError() == WAIT_TIMEOUT here ?
                        ahResult = AHErrorCommsFailure;
                    }
                }
            } else {
                ahResult = AHErrorCommsFailure;
            }
        }

        CloseHandle(overlapInfo.hEvent);
        return ahResult;
    }
}

#endif // _WIN32

#ifdef __APPLE__
#include "TargetConditionals.h"
#ifdef TARGET_OS_MAC

#include <stdio.h>
#include <fcntl.h>
#include <poll.h>
#include <unistd.h>
#include <errno.h>

static FILE* appPipe = 0;
static int appPipeFD = 0;

static int initAppConnection() {
    if (appPipe != 0) {
        return 0;
    }

    appPipe = popen("./allihoopa -pipe", "r+");
    if (appPipe == 0){
        return AHErrorLaunchFailure;
    }
    appPipeFD = fileno(appPipe);
    if(fcntl(appPipeFD, F_SETFL, O_NONBLOCK) == -1) {
        // The app will be launched and left running
        return AHErrorLaunchFailure;
    }

    return 0;
}

static int waitForData() {
    struct pollfd pollFor = {
        appPipeFD,
        POLLIN,
        POLLIN
    };

    int timeoutMS = 1000 * 5;
    int pollResult = poll(&pollFor, 1, timeoutMS);

    if (pollResult == 1) {
        // one descriptor ready!
        return 0;
    }
    else if(pollResult == 0){
        TRACE("poll timeout!");
        return AHErrorCommsFailure;
    }
    else {
        TRACE("poll failed!");
        return AHErrorCommsFailure;
    }
}

static int readFromApp(char* data, size_t length) {
    int result = initAppConnection();
    if (result) {
        return result;
    }
    
    size_t totalBytesRead = 0;

    do {
        size_t bytesWanted = length - totalBytesRead;
        size_t readResult = read(appPipeFD, &data[totalBytesRead], bytesWanted);

        if (readResult != bytesWanted) {
            if (readResult == 0) {
                // EOF!?
                pclose(appPipe);
                appPipe = 0;
                appPipeFD = 0;
                return AHErrorCommsFailure;
            }
            else if (readResult == -1) {
                if (errno == EAGAIN) {
                    int waitResult = waitForData();
                    if (waitResult) {
                        return waitResult;
                    }
                }
                else if (errno == EINVAL) {
                    return AHErrorCommsFailure;
                }
                else if (errno == EFAULT) {
                    return AHErrorUnknownError;
                }
                else {
                    return AHErrorCommsFailure;
                }
            }
            else {
                totalBytesRead += readResult;
            }
        } else {
            totalBytesRead += readResult;
        }
    }while(totalBytesRead != length);

    return 0;
}

static int writeToApp(const char* data, size_t length) {
    int result = initAppConnection();
    if (result) {
        return result;
    }

    size_t bytesWritten = write(appPipeFD, data, length);
    if (bytesWritten != length) {
        return AHErrorCommsFailure;
    }
    return 0;
}

#endif // TARGET_OS_MAC
#endif // DARWIN
