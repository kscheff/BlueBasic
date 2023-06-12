/*
* rapid.h definitions for the WCS Rapid LBR 60 Ladebooster
* (C) 2022 Kai Scheffer, Switzerland
* kai@blue-battery.com
*/

#ifndef RAPID_H
#define RAPID_H

#ifdef __cplusplus
extern "C"
{
#endif


typedef struct
{
  unsigned Notbetrieb : 1;
  unsigned EEPROMSchreibeParameter : 1;
  unsigned EEPROMLeseParameter : 1;
  unsigned EEPROMSchreibeBenutzerparameter : 1;
  unsigned EEPROMLeseBenutzerparameter : 1;
  unsigned TastenInit : 1;
  unsigned TastendruckKurz : 1;
  unsigned TasteGehalten : 1;
  unsigned TastendruckLang : 1;
  unsigned Ladestromreduzierung : 1;        
  unsigned LadestromAnzeige : 1;
  unsigned Freigabe : 1;
  unsigned BoosterUnterbrecherkontaktKfzShunt : 1;
  unsigned Inbetriebnahme:1;
  unsigned TBusSensorGelerntAnzeige: 1;
  unsigned Reserve1:1;        
  unsigned Reserve2: 8;
  unsigned Reserve3:8;
} TypFlags;

typedef struct
{
  unsigned char StartAdresse; // contains last command echo, e.g. 'S'
  float SlaveStromAufbaubatterie;
  float SpannungStartbatterieMessleitung;
  float BenutzerladestromRueckmeldung;
  float SpannungAufbaubatterieMessleitung;
  float SpannungDPlusMessleitung;
  float SpannungStartbatterieIntern;
  float SpannngAufbaubatterieIntern;       
  TypFlags Flags;
  float ReferenzUeberspannungsschutzStartbatterie;
  float SlaveStromStartbatterie;
  float ReferenzUeberspannungsschutzAufbaubatterie;
  float ReferenzUeberstromschutz;
  float SpannungGatetreiber;
  float ReserveFloat1;
  float ReserveFloat2;
  unsigned char SlaveModus;
  unsigned char SlaveSchutzschaltung;
  unsigned int ReserveInt1;
  float StromStartbatterie;
  float StromAufbaubatterie;
  float TemperaturIntern;
  float Effektivitaet;
  float ExterneTemperatursensoren[8];
  unsigned int PwmAufbaubatterie;
  unsigned int PwmStartbatterie;
  unsigned char Modus;
  unsigned char Schutzschaltung;
  float dUdtStart;
  float dUdtStop;
  unsigned int CRC16;
} TypMesswerte;

// prototypes
//bool open_rapid();
//void close_rapid();
void process_rapid(uint8 port, uint8 len);

#ifdef __cplusplus
}
#endif

#endif /* RAPID_H */
