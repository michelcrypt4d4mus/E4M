typedef struct {
    unsigned long state[5];
    unsigned long count[2];
    unsigned char buffer[64];
} SHA1_CTX;

#define SHA_BLOCKSIZE   64
#define SHA_DIGESTSIZE  20

void SHA1TRANSFORM ( unsigned long state [5 ], unsigned char buffer [64 ]);
void SHA1Init ( SHA1_CTX *context );
void SHA1Update ( SHA1_CTX *context , unsigned char *data , unsigned int len );
void SHA1Final ( unsigned char digest [20 ], SHA1_CTX *context );
