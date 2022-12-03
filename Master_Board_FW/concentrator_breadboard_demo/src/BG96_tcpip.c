#include "BG96_tcpip.h"

static uint8_t pdpContextActivated = 0;

void BG96_tcpipConfigParams(void)
{
    // static char* paramsArr[MAX_PARAMS];
    // static uint8_t idx = 0;
    // char tempStr[8];
    // static ContextType_t context_type = IP_V4;

    // idx = 0;
    // paramsArr[idx++] = contextIdStr;
    // sprintf(tempStr, "%d", context_type);
    // paramsArr[idx++] = tempStr;
    // paramsArr[idx++] = VODAFONE_APN_STR;
    // prepareArg(paramsArr, idx, AT_configureParametersOfTcpIpContext.arg);
    // queueAtPacket(&AT_configureParametersOfTcpIpContext, WRITE_COMMAND); 

    // idx = 0;
    // paramsArr[idx++] = contextIdStr;
    // prepareArg(paramsArr, idx, AT_activatePDPContext.arg);
    // queueAtPacket(&AT_activatePDPContext, WRITE_COMMAND);

    static void* paramsArr[MAX_PARAMS];
    static uint8_t idx = 0;
    static ContextType_t context_type = IP_V4;
    
    idx = 0;
    paramsArr[idx++] = (void*) (&contextID);
    paramsArr[idx++] = (void*) (&context_type);
    paramsArr[idx++] = (void*) VODAFONE_APN_STR;
    prepAtCmdArgs(AT_configureParametersOfTcpIpContext.arg, paramsArr, idx);
    queueAtPacket(&AT_configureParametersOfTcpIpContext, WRITE_COMMAND); 

    idx = 0;
    paramsArr[idx++] = (void*) (&contextID);
    prepAtCmdArgs(AT_activatePDPContext.arg, paramsArr, idx);
    queueAtPacket(&AT_activatePDPContext, WRITE_COMMAND);   

    queueAtPacket(&AT_activatePDPContext, READ_COMMAND);

}

void BG96_tcpipResponseParser(BG96_AtPacket_t* packet, char* data)
{
    BG96_AtPacket_t tempPacket;
    char tempData[BUFFER_SIZE];

    memcpy(&tempPacket, packet, sizeof(BG96_AtPacket_t));
    memcpy(tempData, data, BUFFER_SIZE);

    switch (tempPacket.atCmd->id)
    {
        case CONFIGURE_PARAMETERS_OF_A_TCPIP_CONTEXT:

            break;
        case ACTIVATE_A_PDP_CONTEXT:
            if (tempPacket.atCmdType == WRITE_COMMAND)
            {
                dumpInfo("PDP Context: [ACTIVATED]\r\n");
                pdpContextActivated = 1;
            }
            break;
        case DEACTIVATE_A_PDP_CONTEXT:
            if (tempPacket.atCmdType == WRITE_COMMAND)
            {
                dumpInfo("PDP Context: [DEACTIVATED]\r\n");
                pdpContextActivated = 0;
            }
            break;
        default:
            break;
    }
}
