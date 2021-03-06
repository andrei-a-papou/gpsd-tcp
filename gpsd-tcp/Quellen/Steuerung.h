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

#ifndef STEUERUNG_H
#define STEUERUNG_H

#include <QtCore>
#include "Meldung.h"

enum Protokolltiefe
{
	Fehler,
	Info,
	Debug
};

class QTcpSocket;
class QTcpServer;
class Steuerung : public QObject
{
	Q_OBJECT
	public:
		explicit			Steuerung(QObject *eltern = Q_NULLPTR);
							~Steuerung();
		static void			Signal_SIGTERM_Verwaltung(int);

	public Q_SLOTS:
		void				beenden();

	Q_SIGNALS:
		void				SensorenAbschalten();

	private Q_SLOTS:
		void				loslegen();
		void				NeuerKlient(QObject *dienst);
		void				KlientLoeschen(QObject *klient);
		void				DatenVerteilen(const QString &daten);
		void				Melden(Meldung meldung) const;
		void				SensorenAbgeschaltet();

	private:
		bool				TCPstarten();
		bool				KontextWechseln(const QString &nutzer, const QString &gruppe);
		bool				ModulLaden(const QString modulname,const QString &pfad);
		QSettings			*K_Einstellungen;
		Protokolltiefe		K_Protokoll;
		Protokolltiefe		ProtokollTextNachZahl(const QString &text) const;
		QSignalMapper		*K_Klientensammler;
		QSignalMapper		*K_Klientloescher;
		QList<QTcpSocket*>	*K_Klienten;
		QSocketNotifier		*K_SocketBeenden;
		static int			SIGTERM_Socked[2];

};

#endif // STEUERUNG_H
