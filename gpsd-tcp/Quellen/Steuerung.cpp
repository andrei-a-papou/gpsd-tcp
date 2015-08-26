/*
	Copyright (C) 2015 Frank Büttner frank@familie-büttner.de

	This program is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 3 of the License, or
	(at your option) any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with this program.  If not, see <http://www.gnu.org/licenses/>
*/

#include "Steuerung.h"
#include "Vorgaben.h"

#include <QtNetwork>

#include <systemd/sd-journal.h>
#include <errno.h>
#include <grp.h>
#include <pwd.h>

Steuerung::Steuerung(QObject *eltern) : QObject(eltern)
{
	K_Klienten=Q_NULLPTR;
	K_Einstellungen=new QSettings(KONFIGDATEI,QSettings::IniFormat,this);
	connect(QCoreApplication::instance(),SIGNAL(aboutToQuit()),this,SLOT(beenden()));
	QTimer::singleShot(0,this,SLOT(loslegen()));
}
Steuerung::~Steuerung()
{
	if(K_Klienten)
	{
		for( auto Klient : *K_Klienten)
			Klient->disconnectFromHost();
	}
}

void Steuerung::loslegen()
{
	Melden(Meldung("a475b92d2cc84b63a233e7a027442c5f",tr("Starte ...")));
	K_Klienten = new QList<QTcpSocket*>;
	K_Protokoll=ProtokollTextNachZahl(K_Einstellungen->value("Protokollebene","Info").toString());
	K_Modulpfad=K_Einstellungen->value("Modulpfad",MODULE).toString();
	K_Modul=K_Einstellungen->value("Modul",MODUL).toString();
	if (K_Protokoll==Protokolltiefe::Debug)
	{
		Melden(Meldung("35b01b5da0cc44dcb04822de636f620b",tr("Modulpfad: %1").arg(K_Modulpfad),LOG_DEBUG));
		Melden(Meldung("9ac70e7688f8437eb1b72e6737b4c445",tr("Modul: %1").arg(K_Modul),LOG_DEBUG));
	}
	int Anschluss=0;
	QString Adresse;
	QString Fehlertext;
	QString GruppeName=K_Einstellungen->value("Gruppe",GRUPPE).toString();
	QString NutzerName=K_Einstellungen->value("Benutzer",BENUTZER).toString();
	int NutzerID;
	int GruppeID;
	group *Gruppe=getgrnam(GruppeName.toUtf8().constData());
	int FehlerGruppe=errno;
	passwd *Nutzer=getpwnam(NutzerName.toUtf8().constData());
	int FehlerNutzer=errno;
	if( Gruppe==NULL)
	{
		Fehlertext=trUtf8("Gruppenname: %1 konnte nicht aufgelöst werden.").arg(GruppeName);
		if (FehlerGruppe !=0)
			Fehlertext.append(QString("\n%1").arg(strerror(FehlerGruppe)));
		Melden(Meldung("3dfabfae6cf244b690d9d41b0b293593",Fehlertext,LOG_CRIT));
		QCoreApplication::quit();
		return;
	}
	if( Nutzer==NULL)
	{
		Fehlertext=trUtf8("Nutzername: %1 konnte nicht aufgelöst werden.").arg(NutzerName);
		if (FehlerNutzer!=0)
			Fehlertext.append(QString("\n%1").arg(strerror(FehlerNutzer)));
		Melden(Meldung("a9a1690d05ef442cbd2625d3065bbdb9",Fehlertext,LOG_CRIT));
		QCoreApplication::quit();
		return;
	}
	GruppeID=Gruppe->gr_gid;
	NutzerID=Nutzer->pw_uid;

	if (K_Protokoll==Protokolltiefe::Debug)
		Melden(Meldung("b044be0993314c3384c95cce368c207e",tr("Starte als Nutzer: %1(%2) Gruppe: %3(%4)").arg(NutzerName).arg(NutzerID).arg(GruppeName).arg(GruppeID),LOG_DEBUG));

	QTcpServer *Datendienst=Q_NULLPTR;
	K_Klientensammler=new QSignalMapper(this);
	for( auto Dienst : K_Einstellungen->childGroups())
	{
		if(Dienst.toUpper().startsWith("DIENST"))
		{
			Anschluss=K_Einstellungen->value(QString("%1/Anschluss").arg(Dienst),0).toInt();
			if ((Anschluss ==0) || (Anschluss >65535))
			{
				if(K_Protokoll >=Protokolltiefe::Fehler)
					Melden(Meldung("ba896cda507d4a79a34c6b7db175b64e",trUtf8("Anschlussnummer %1 ist ungültig. Ignoriere %2.").arg(Anschluss).arg(Dienst),LOG_ERR));
				continue;
			}
			Adresse=K_Einstellungen->value(QString("%1/Adresse").arg(Dienst),"").toString();
			if ((Adresse.isEmpty()) || (QHostAddress(Adresse).isNull()))
			{
				if(K_Protokoll >=Protokolltiefe::Fehler)
					Melden(Meldung("bd4b290c2a7a4627a8d2129338b58798",trUtf8("Adresse %1 ist ungültig. Ignoriere %2.").arg(Adresse).arg(Dienst),LOG_ERR));
				continue;
			}
			if(K_Protokoll==Protokolltiefe::Debug)
				Melden(Meldung("81964998fd0f4f6cb7c82ffc5b7bdf27",tr("Erstelle: %1 Adresse: %2 Anschluss: %3").arg(Dienst).arg(Adresse).arg(Anschluss),LOG_DEBUG));

			//Starten der Dienste
			Datendienst=new QTcpServer(this);
			if(!Datendienst->listen(QHostAddress(Adresse),Anschluss))
			{
				Melden(Meldung("d91c632a84b54f3cb634485cf007d485",tr("Konnte %1 nicht starten.\n%2").arg(Dienst).arg(Datendienst->errorString()),LOG_ERR));
				Datendienst->deleteLater();
			}
			else
			{
				connect(Datendienst, SIGNAL(newConnection()), K_Klientensammler, SLOT(map()));
				K_Klientensammler->setMapping(Datendienst,Datendienst);
				if(K_Protokoll >=Protokolltiefe::Info)
					Melden(Meldung("fefe966c7e594a48bd0365e961a2c30c",trUtf8("Lausche für %1 auf %2 Anschluss %3").arg(Dienst).arg(Adresse).arg(Anschluss),LOG_INFO));
			}

		}
	}
	connect(K_Klientensammler,SIGNAL(mapped(QObject*)),this,SLOT(NeuerKlient(QObject*)));
	//Modul laden

	//Benutzer wechseln

	/*if(setuid()!=0)
		text=strerror_l(errno)
	if(setgid()!=0)
			 strerror_l(errno)'*/


}
void Steuerung::beenden()
{
	Melden(Meldung("3c9ac521e2e6487995ac623d35b06d70",tr("Beende ...")));
}

void Steuerung::Melden(Meldung m) const
{
	 sd_journal_send(QString("MESSAGE=%1").arg(m.TextHolen()).toUtf8().constData(),QString("MESSAGE_ID=%1").arg(m.IDHolen()).toUtf8().constData(),
					 QString("PRIORITY=%1").arg(m.PrioritaetHolen()).toUtf8().constData(),QString("VERSION=%1").arg(VERSION).toUtf8().constData(),NULL);
}
Protokolltiefe Steuerung::ProtokollTextNachZahl(const QString &text) const
{
	if(text.toUpper().contains("INFO"))
		return Protokolltiefe::Info;
	else if((text.toUpper().contains("FEHLER")) || (text.toUpper().contains("ERROR")))
		return Protokolltiefe::Fehler;
	else if(text.toUpper().contains("DEBUG"))
		return Protokolltiefe::Debug;
	Melden(Meldung("45de11448ff94ec9a17b7a549bcac339",trUtf8("Ungültige Protokolltiefe %1, benutze Info.").arg(text),LOG_ERR));
	return Protokolltiefe::Info;
}
void Steuerung::NeuerKlient(QObject *dienst)
{
	QTcpSocket* Klient =dynamic_cast<QTcpServer*> (dienst)->nextPendingConnection();
	if (K_Protokoll >= Protokolltiefe::Info)
		Melden(Meldung("a32a5261338d422d8e27dc832e1c6e90",tr("Verbidung von %1").arg(Klient->peerAddress().toString()),LOG_INFO));
	K_Klienten->append(Klient);
}
