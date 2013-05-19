#include "NFCLinkLayer.h"

NFCLinkLayer::NFCLinkLayer(NFCReader *nfcReader)
    : _nfcReader(nfcReader)
{
}

NFCLinkLayer::~NFCLinkLayer()
{

}


uint32_t NFCLinkLayer::openNPPClientLink(boolean debug)
{
   PDU *targetPayload;
   PDU *recievedPDU;
   uint8_t PDUBuffer[64];
   uint8_t DataIn[64];
   
   targetPayload = (PDU *) PDUBuffer;
   
   if (debug)
   {
      Serial.println(F("Opening NPP Client Link."));
   }
   uint32_t result = _nfcReader->configurePeerAsTarget(SNEP_CLIENT);

   if (IS_ERROR(result))
   {
       return result;
   }
   
   recievedPDU = ( PDU *) DataIn;
   uint32_t rx_result = _nfcReader->targetRxData(DataIn);
   if (IS_ERROR(rx_result))
   {
     if (debug)
     {
       Serial.println(F("Connection Failed."));
     }
     return CONNECT_RX_FAILURE;
   }

  uint8_t PDU[2] ;

  PDU[0] = 0x11;
  PDU[1] = 0x20;
  if (IS_ERROR(_nfcReader->targetTxData(PDU, 2))) {
    return CONNECT_TX_FAILURE;
  }
  
  _nfcReader->targetRxData(DataIn);
  
  PDU[0] = 0;
  PDU[1] = 0;
  _nfcReader->targetTxData(PDU, 2);
  _nfcReader->targetRxData(DataIn);


   if (recievedPDU->getPTYPE() != CONNECTION_COMPLETE_PTYPE)
   {
      if (debug)
      {
         Serial.println(F("Connection Complete Failed."));
      }
      return UNEXPECTED_PDU_FAILURE;
   }

   DSAP = recievedPDU->getSSAP();
   SSAP = recievedPDU->getDSAP();

   return RESULT_SUCCESS;
}

uint32_t NFCLinkLayer::closeNPPClientLink()
{

}


uint32_t NFCLinkLayer::openNPPServerLink(boolean debug)
{
   uint8_t status[2];
   uint8_t DataIn[64];
   PDU *recievedPDU;
   PDU targetPayload;
   uint32_t result;

   if (debug)
   {
      Serial.println(F("Opening Server Link."));
   }

   result = _nfcReader->configurePeerAsTarget(SNEP_CLIENT);
   if (IS_ERROR(result))
   {
       return result;
   }

   uint8_t symm[] = {0, 0};
   recievedPDU = (PDU *)DataIn;
   do
   {
     result = _nfcReader->targetRxData(DataIn);
     
     if (IS_ERROR(result))
     {
        return result;
     }
     
     if (recievedPDU->getPTYPE() == CONNECT_PTYPE) {
        break;
     } else {
        result = _nfcReader->targetTxData(symm, sizeof(symm));
     }
     
   } while (RESULT_OK(result));

   targetPayload.setDSAP(recievedPDU->getSSAP());
   targetPayload.setPTYPE(CONNECTION_COMPLETE_PTYPE);
   targetPayload.setSSAP(recievedPDU->getDSAP());

   if (IS_ERROR(_nfcReader->targetTxData((uint8_t *)&targetPayload, 2)))
   {
      if (debug)
      {
         Serial.println(F("Connection Complete Failed."));
      }
      return CONNECT_COMPLETE_TX_FAILURE;
   }

   uint8_t zero[] = {0, 0};
   _nfcReader->targetTxData(zero, sizeof(zero));

   return RESULT_SUCCESS;
}

uint32_t NFCLinkLayer::closeNPPServerLink()
{
   uint8_t DataIn[64];
   PDU *recievedPDU;

   recievedPDU = (PDU *)DataIn;

   uint32_t result = _nfcReader->targetRxData(DataIn);

   if (_nfcReader->isTargetReleasedError(result))
   {
      return RESULT_SUCCESS;
   }
   else if (IS_ERROR(result))
   {
      return result;
   }

   //Serial.println(F("Recieved disconnect Message."));

   return result;
}

uint32_t NFCLinkLayer::serverLinkRxData(uint8_t *&Data, boolean debug)
{
   uint32_t result = _nfcReader->targetRxData(Data);
   uint8_t len;
   PDU *recievedPDU;
   PDU ackPDU;

   if (IS_ERROR(result))
   {
      if (debug)
      {
         Serial.println(F("Failed to Recieve NDEF Message."));
      }
      return NDEF_MESSAGE_RX_FAILURE;
   }

   len = (uint8_t) result;

   recievedPDU = (PDU *) Data;

   if (recievedPDU->getPTYPE() != INFORMATION_PTYPE)
   {
      if (debug)
      {
         Serial.println(F("Unexpected PDU"));
      }
      return UNEXPECTED_PDU_FAILURE;
   }

   // Acknowledge reciept of Information PDU
   ackPDU.setDSAP(recievedPDU->getSSAP());
   ackPDU.setPTYPE(RECEIVE_READY_TYPE);
   ackPDU.setSSAP(recievedPDU->getDSAP());

   ackPDU.params.sequence = recievedPDU->params.sequence & 0x0F;

   result = _nfcReader->targetTxData((uint8_t *)&ackPDU, 3);
   if (IS_ERROR(result))
   {
      if (debug)
      {
         Serial.println(F("Ack Failed."));
      }
      return result;
   }


   Data = &Data[3];

   return len - 2;
}

uint32_t NFCLinkLayer::clientLinkTxData(uint8_t *snepMessage, uint32_t len, boolean debug)
{
   PDU *infoPDU = (PDU *) ALLOCATE_HEADER_SPACE(snepMessage, 3);
   infoPDU->setDSAP(DSAP);
   infoPDU->setSSAP(SSAP);
   infoPDU->setPTYPE(INFORMATION_PTYPE);

   infoPDU->params.sequence = 0;
      
   uint8_t buf[32];
      
    uint8_t symm[] = {0, 0};
    _nfcReader->targetTxData(symm, sizeof(symm));
    
//    _nfcReader->targetRxData(buf);
   

   if (IS_ERROR(_nfcReader->targetTxData((uint8_t *)infoPDU, len + 3)))
   {
     if (debug)
     {
        Serial.println(F("Sending NDEF Message Failed."));
     }
     return NDEF_MESSAGE_TX_FAILURE;
   }
   

   _nfcReader->targetRxData(buf);
   
   _nfcReader->targetTxData(symm, sizeof(symm));

   PDU disconnect;
   disconnect.setDSAP(DSAP);
   disconnect.setSSAP(SSAP);
   disconnect.setPTYPE(DISCONNECT_PTYPE);
   
   _nfcReader->targetTxData((uint8_t *)&disconnect, 2);
   
   _nfcReader->targetRxData(buf);

   if (debug)
   {
      Serial.println(F("Sent NDEF Message"));
   }
   return RESULT_SUCCESS;
}

inline bool PDU::isConnectClientRequest()
{
#if 0
    return ((getPTYPE() == CONNECT_PTYPE)                     &&
             (params.length == CONNECT_SERVICE_NAME_LEN)       &&
             (strncmp((char *)params.data, CONNECT_SERVICE_NAME, CONNECT_SERVICE_NAME_LEN) == 0));
#else
    return (getPTYPE() == CONNECT_PTYPE);
#endif
}

PDU::PDU()
{
   field[0] = 0;
   field[1] = 0;
   params.type = 0;
   params.length = 0;
}

uint8_t PDU::getDSAP()
{
   return (field[0] >> 2);
}

uint8_t PDU::getSSAP()
{
   return (field[1] & 0x3F);
}

uint8_t PDU::getPTYPE()
{
   return (((field[0] & 0x03) << 2) | ((field[1] & 0xC0) >> 6));
}

void PDU::setDSAP(uint8_t DSAP)
{
   field[0] &= 0x03; // Clear the DSAP bits
   field[0] |= ((DSAP & 0x3F) << 2);  // Set the bits
}

void PDU::setSSAP(uint8_t SSAP)
{
  field[1] &= (0xC0);    // Clear the SSAP bits
  field[1] |= (0x3F & SSAP);
}

void PDU::setPTYPE(uint8_t PTYPE)
{
   field[0] &= 0xFC; // Clear the last two bits that contain the PTYPE
   field[1] &= 0x3F; // Clear the upper two bits that contain the PTYPE
   field[0] |= (PTYPE & 0x0C) >> 2;
   field[1] |= (PTYPE & 0x03) << 6;
}
