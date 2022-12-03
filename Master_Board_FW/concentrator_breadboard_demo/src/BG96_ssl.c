#include "BG96_ssl.h"

void BG96_sslConfigParams(void)
{
    static void* paramsArr[MAX_PARAMS];
    static uint8_t idx = 0;
    
    static char cacertpath[] = "\"cacert.pem\"";
    static char client_cert_path[] = "\"client.pem\"";
    static char client_key_path[] = "\"user_key.pem\"";
    static char supportAllCiphersuites[] = "0xFFFF";
    static SslVersion_t SSL_version = ALL;
    static SslSecLevel_t seclevel = MANAGE_SERVER_AND_CLIENT_AUTHENTICATION;
    static SslIgnoreLocalTime_t ignore_ltime = IGNORE_VALIDITY_CHECK;
    
    idx = 0;
    paramsArr[idx++] = (void*) "\"cacert\"";
    paramsArr[idx++] = (void*) &SSL_ctxID;
    paramsArr[idx++] = (void*) cacertpath;
    prepAtCmdArgs(AT_configureParametersOfSSLContext.arg, paramsArr, idx);
    queueAtPacket(&AT_configureParametersOfSSLContext, WRITE_COMMAND); 

    idx = 0;
    paramsArr[idx++] = (void*) "\"clientcert\"";
    paramsArr[idx++] = (void*) &SSL_ctxID;
    paramsArr[idx++] = (void*) client_cert_path;
    prepAtCmdArgs(AT_configureParametersOfSSLContext.arg, paramsArr, idx);
    queueAtPacket(&AT_configureParametersOfSSLContext, WRITE_COMMAND); 

    idx = 0;
    paramsArr[idx++] = (void*) "\"clientkey\"";
    paramsArr[idx++] = (void*) &SSL_ctxID;
    paramsArr[idx++] = (void*) client_key_path;
    prepAtCmdArgs(AT_configureParametersOfSSLContext.arg, paramsArr, idx);
    queueAtPacket(&AT_configureParametersOfSSLContext, WRITE_COMMAND); 
    

    idx = 0;
    paramsArr[idx++] = (void*) "\"ciphersuite\"";
    paramsArr[idx++] = (void*) &SSL_ctxID;
    paramsArr[idx++] = (void*) supportAllCiphersuites;
    prepAtCmdArgs(AT_configureParametersOfSSLContext.arg, paramsArr, idx);
    queueAtPacket(&AT_configureParametersOfSSLContext, WRITE_COMMAND); 

    idx = 0;
    paramsArr[idx++] = (void*) "\"sslversion\"";
    paramsArr[idx++] = (void*) &SSL_ctxID;
    paramsArr[idx++] = (void*) &SSL_version;
    prepAtCmdArgs(AT_configureParametersOfSSLContext.arg, paramsArr, idx);
    queueAtPacket(&AT_configureParametersOfSSLContext, WRITE_COMMAND);

    idx = 0;
    paramsArr[idx++] = (void*) "\"seclevel\"";
    paramsArr[idx++] = (void*) &SSL_ctxID;
    paramsArr[idx++] = (void*) &seclevel;
    prepAtCmdArgs(AT_configureParametersOfSSLContext.arg, paramsArr, idx);
    queueAtPacket(&AT_configureParametersOfSSLContext, WRITE_COMMAND);

    idx = 0;
    paramsArr[idx++] = (void*) "\"ignorelocaltime\"";
    paramsArr[idx++] = (void*) &SSL_ctxID;
    paramsArr[idx++] = (void*) &ignore_ltime;
    prepAtCmdArgs(AT_configureParametersOfSSLContext.arg, paramsArr, idx);
    queueAtPacket(&AT_configureParametersOfSSLContext, WRITE_COMMAND);
}