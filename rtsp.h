#define MTH_DESCRIBE        0
#define MTH_ANNOUNCE        1
#define MTH_GET_PARAMETER   2
#define MTH_OPTIONS         3
#define MTH_PAUSE           4
#define MTH_PLAY            5
#define MTH_RECORD          6
#define MTH_REDIRECT        7
#define MTH_SETUP           8
#define MTH_SET_PARAMETER   9
#define MTH_TEARDOWN        10

typedef struct {
    int cseq;
    char *accept;
    char *user_agent;
} Request_Header;

typedef struct {
    int method;
    char *url;
    char version[33];
    Request_Header rh;
} Rtsp_Request;
