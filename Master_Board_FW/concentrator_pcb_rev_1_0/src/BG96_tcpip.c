#include "BG96_tcpip.h"

static uint8_t pdpContextActivated = 0;

void BG96_tcpipConfigParams(void)
{
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

uint8_t BG96_tcpipResponseParser(BG96_AtPacket_t* packet, char* data)
{
    BG96_AtPacket_t tempPacket;
    char tempData[BUFFER_SIZE];

    memcpy(&tempPacket, packet, sizeof(BG96_AtPacket_t));
    memcpy(tempData, data, BUFFER_SIZE);

    switch (tempPacket.atCmd.id)
    {
        case CONFIGURE_PARAMETERS_OF_A_TCPIP_CONTEXT:
            return EXIT_SUCCESS;
            break;
        case ACTIVATE_A_PDP_CONTEXT:
            if (tempPacket.atCmdType == WRITE_COMMAND)
            {
                dumpInfo("PDP Context: [ACTIVATED]\r\n");
                pdpContextActivated = 1;
                return EXIT_SUCCESS;
            }
            return EXIT_SUCCESS;
            break;
        case DEACTIVATE_A_PDP_CONTEXT:
            if (tempPacket.atCmdType == WRITE_COMMAND)
            {
                dumpInfo("PDP Context: [DEACTIVATED]\r\n");
                pdpContextActivated = 0;
                return EXIT_SUCCESS;
            }
            return EXIT_SUCCESS;
            break;
        default:
            dumpDebug("Unknown/Not implemented TCPIP command id.\r\n");
            return EXIT_SUCCESS;
            break;
    }
    return EXIT_FAILURE;
}
