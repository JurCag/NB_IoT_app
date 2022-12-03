#include "BG96_ssl.h"

void BG96_sslConfigParams(void)
{
    static char* paramsArr[MAX_PARAMS];
    static uint8_t idx = 0;
    char tempStr[8];
    
    static SslVersion_t SSL_version = ALL;
    static SslSecLevel_t seclevel = MANAGE_SERVER_AND_CLIENT_AUTHENTICATION;
    static SslIgnoreLocalTime_t ignore_ltime = IGNORE_VALIDITY_CHECK;
    static char cacertpath[] = "\"cacert.pem\"";
    static char client_cert_path[] = "\"client.pem\"";
    static char client_key_path[] = "\"user_key.pem\"";
    static char supportAllCiphersuites[] = "0xFFFF";
    
    idx = 0;
    paramsArr[idx++] = "\"cacert\"";
    paramsArr[idx++] = sslCtxIdStr;
    paramsArr[idx++] = cacertpath;
    prepareArg(paramsArr, idx, AT_configureParametersOfSSLContext.arg);
    queueAtPacket(&AT_configureParametersOfSSLContext, WRITE_COMMAND); 

    idx = 0;
    paramsArr[idx++] = "\"clientcert\"";
    paramsArr[idx++] = sslCtxIdStr;
    paramsArr[idx++] = client_cert_path;
    prepareArg(paramsArr, idx, AT_configureParametersOfSSLContext.arg);
    queueAtPacket(&AT_configureParametersOfSSLContext, WRITE_COMMAND); 

    idx = 0;
    paramsArr[idx++] = "\"clientkey\"";
    paramsArr[idx++] = sslCtxIdStr;
    paramsArr[idx++] = client_key_path;
    prepareArg(paramsArr, idx, AT_configureParametersOfSSLContext.arg);
    queueAtPacket(&AT_configureParametersOfSSLContext, WRITE_COMMAND); 

    idx = 0;
    paramsArr[idx++] = "\"ciphersuite\"";
    paramsArr[idx++] = sslCtxIdStr;
    paramsArr[idx++] = supportAllCiphersuites;
    prepareArg(paramsArr, idx, AT_configureParametersOfSSLContext.arg);
    queueAtPacket(&AT_configureParametersOfSSLContext, WRITE_COMMAND); 

    idx = 0;
    paramsArr[idx++] = "\"sslversion\"";
    paramsArr[idx++] = sslCtxIdStr;
    sprintf(tempStr, "%d", SSL_version);
    paramsArr[idx++] = tempStr;
    prepareArg(paramsArr, idx, AT_configureParametersOfSSLContext.arg);
    queueAtPacket(&AT_configureParametersOfSSLContext, WRITE_COMMAND);

    idx = 0;
    paramsArr[idx++] = "\"seclevel\"";
    paramsArr[idx++] = sslCtxIdStr;
    sprintf(tempStr, "%d", seclevel);
    paramsArr[idx++] = tempStr;
    prepareArg(paramsArr, idx, AT_configureParametersOfSSLContext.arg);
    queueAtPacket(&AT_configureParametersOfSSLContext, WRITE_COMMAND);

    idx = 0;
    paramsArr[idx++] = "\"ignorelocaltime\"";
    paramsArr[idx++] = sslCtxIdStr;
    sprintf(tempStr, "%d", ignore_ltime);
    paramsArr[idx++] = tempStr;
    prepareArg(paramsArr, idx, AT_configureParametersOfSSLContext.arg);
    queueAtPacket(&AT_configureParametersOfSSLContext, WRITE_COMMAND);
}