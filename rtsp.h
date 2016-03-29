#define CMD_DESCRIBE        0
#define CMD_ANNOUNCE        1
#define CMD_GET_PARAMETER   2
#define CMD_OPTIONS         3
#define CMD_PAUSE           4
#define CMD_PLAY            5
#define CMD_RECORD          6
#define CMD_REDIRECT        7
#define CMD_SETUP           8
#define CMD_SET_PARAMETER   9
#define CMD_TEARDOWN        10

typedef struct {
    int cseq;
    char *accept;
    char *user_agent;
} Request_Header;

typedef struct {
    int method;
    char *uri;
    char version[32];
    Request_Header rh;
} Rtsp_Request;
