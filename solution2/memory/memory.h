#define MAX_X 512
#define MAX_Y 512
#define MAX_Z 16

#define POS(x,y,z) (x * MAX_Y * MAX_Z + y * MAX_Z + z)

extern int rebuild_memory(void *arg);
extern void destroy_memory();
