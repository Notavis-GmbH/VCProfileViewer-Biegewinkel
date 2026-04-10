// =============================================================================
// licensemanager.cpp
// Vollständige Implementierung aller Lizenzverwaltungsmethoden.
// Nutzt QNetworkAccessManager mit lokalem QEventLoop für synchrone API-Aufrufe.
// =============================================================================

#include "licensemanager.h"
#include "keygen_config.h"

#include <QCryptographicHash>
#include <QNetworkInterface>
#include <QSysInfo>
#include <QSettings>
#include <QEventLoop>
#include <QTimer>
#include <QLoggingCategory>
#include <QNetworkRequest>
#include <QUrl>
#include <QJsonArray>

// Logging-Kategorie für alle Lizenz-bezogenen Meldungen
Q_LOGGING_CATEGORY(licenseLog, "app.license")

// ---------------------------------------------------------------------------
// Konstruktor
// ---------------------------------------------------------------------------
LicenseManager::LicenseManager(QObject* parent)
    : QObject(parent)
    , m_networkManager(new QNetworkAccessManager(this))
{
    // TLS-Fehler ignorieren in Entwicklung — in Produktion entfernen!
    // connect(m_networkManager, &QNetworkAccessManager::sslErrors, ...);
}

// ---------------------------------------------------------------------------
// generateFingerprint()
// Kombiniert MAC-Adresse + CPU-Modell + Hostname und bildet SHA-256 Hash.
// Das Ergebnis ist ein hex-kodierter String (64 Zeichen).
// ---------------------------------------------------------------------------
QString LicenseManager::generateFingerprint()
{
    // Zwischengespeicherten Fingerabdruck zurückgeben falls vorhanden
    if (!m_cachedFingerprint.isEmpty()) {
        return m_cachedFingerprint;
    }

    // --- 1. MAC-Adresse: erste nicht-Loopback Interface ---
    QString macAddress;
    const auto interfaces = QNetworkInterface::allInterfaces();
    for (const QNetworkInterface& iface : interfaces) {
        // Loopback und virtuelle Interfaces überspringen
        if (iface.flags().testFlag(QNetworkInterface::IsLoopBack)) {
            continue;
        }
        const QString hwAddr = iface.hardwareAddress();
        if (!hwAddr.isEmpty() && hwAddr != "00:00:00:00:00:00") {
            macAddress = hwAddr;
            break;
        }
    }

    // Fallback falls keine MAC-Adresse gefunden
    if (macAddress.isEmpty()) {
        macAddress = "00:00:00:00:00:00";
        qCWarning(licenseLog) << "Keine MAC-Adresse gefunden, verwende Fallback.";
    }

    // --- 2. CPU-Modell aus Qt-Systeminformationen ---
    // QSysInfo::currentCpuArchitecture() gibt z.B. "x86_64", "arm64"
    // Für detailliertere CPU-Info plattformspezifisch erweitern
    const QString cpuModel = QSysInfo::currentCpuArchitecture()
                           + QLatin1Char('/')
                           + QSysInfo::buildCpuArchitecture();

    // --- 3. Rechnername (Hostname) ---
    const QString hostname = QSysInfo::machineHostName();

    // --- 4. Alle Komponenten zusammenführen und SHA-256 bilden ---
    const QString combined = macAddress
                           + QLatin1Char('|')
                           + cpuModel
                           + QLatin1Char('|')
                           + hostname;

    qCDebug(licenseLog) << "Fingerabdruck-Eingabe:"
                        << "MAC=" << macAddress
                        << "CPU=" << cpuModel
                        << "Host=" << hostname;

    const QByteArray hash = QCryptographicHash::hash(
        combined.toUtf8(),
        QCryptographicHash::Sha256
    );

    m_cachedFingerprint = QString::fromLatin1(hash.toHex());
    qCInfo(licenseLog) << "Hardware-Fingerabdruck generiert:" << m_cachedFingerprint.left(16) << "...";

    return m_cachedFingerprint;
}

// ---------------------------------------------------------------------------
// checkLicense()
// Liest gespeicherte Lizenz aus QSettings, validiert online gegen Keygen.sh.
// Gibt den aktuellen LicenseStatus zurück.
// ---------------------------------------------------------------------------
LicenseStatus LicenseManager::checkLicense()
{
    QSettings settings;
    const QString licenseKey  = settings.value(KeygenConfig::SETTINGS_LICENSE_KEY).toString();
    const QString licenseType = settings.value(KeygenConfig::SETTINGS_LICENSE_TYPE).toString();

    // Keine Lizenz vorhanden → Dialog benötigt
    if (licenseKey.isEmpty()) {
        qCInfo(licenseLog) << "Keine gespeicherte Lizenz gefunden.";
        return LicenseStatus::NOT_ACTIVATED;
    }

    const QString fingerprint = generateFingerprint();

    // Lizenzschlüssel gegen Keygen.sh validieren (inkl. Fingerabdruck-Scope)
    bool isValid = false;
    QString validationCode;
    const QString licenseId = validateLicenseKey(licenseKey, fingerprint, isValid, validationCode);

    qCInfo(licenseLog) << "Validierungsergebnis:"
                       << "valid=" << isValid
                       << "code=" << validationCode
                       << "type=" << licenseType;

    // --- Ergebnis auswerten ---

    if (isValid) {
        // Lizenz gültig → nach Typ unterscheiden
        if (licenseType == QLatin1String("trial")) {
            const int daysLeft = trialDaysRemaining();
            if (daysLeft > 0) {
                qCInfo(licenseLog) << "Trial aktiv," << daysLeft << "Tage verbleibend.";
                return LicenseStatus::TRIAL_ACTIVE;
            } else {
                qCWarning(licenseLog) << "Trial-Lizenz lokal abgelaufen.";
                return LicenseStatus::TRIAL_EXPIRED;
            }
        }
        return LicenseStatus::VALID;
    }

    // Validierungscodes von Keygen.sh auswerten:
    // https://keygen.sh/docs/api/licenses/#licenses-actions-validate-key
    if (validationCode == QLatin1String("EXPIRED")) {
        if (licenseType == QLatin1String("trial")) {
            return LicenseStatus::TRIAL_EXPIRED;
        }
        return LicenseStatus::EXPIRED;
    }

    if (validationCode == QLatin1String("NO_MACHINES")
     || validationCode == QLatin1String("NO_MACHINE")
     || validationCode == QLatin1String("FINGERPRINT_SCOPE_MISMATCH")) {
        // Lizenz existiert, aber diese Maschine ist nicht aktiviert
        return LicenseStatus::NOT_ACTIVATED;
    }

    // Alle anderen Fälle (SUSPENDED, BANNED, NOT_FOUND, Netzwerkfehler)
    m_lastError = validationCode;
    return LicenseStatus::ERROR;
}

// ---------------------------------------------------------------------------
// activateCommercialLicense()
// 1. Validiert den eingegebenen Lizenzschlüssel → holt Lizenz-ID
// 2. Aktiviert diese Maschine (POST /machines) mit dem Lizenzschlüssel als Auth
// Gibt true zurück bei Erfolg.
// ---------------------------------------------------------------------------
bool LicenseManager::activateCommercialLicense(const QString& licenseKey)
{
    qCInfo(licenseLog) << "Starte kommerzielle Lizenzaktivierung...";

    const QString fingerprint = generateFingerprint();

    // --- Schritt 1: Lizenzschlüssel validieren und ID ermitteln ---
    bool isValid = false;
    QString validationCode;
    const QString licenseId = validateLicenseKey(licenseKey, fingerprint, isValid, validationCode);

    // Bereits auf dieser Maschine aktiviert → als Erfolg werten
    if (isValid) {
        qCInfo(licenseLog) << "Lizenz bereits auf dieser Maschine aktiviert.";
        QSettings settings;
        settings.setValue(KeygenConfig::SETTINGS_LICENSE_KEY,  licenseKey);
        settings.setValue(KeygenConfig::SETTINGS_LICENSE_ID,   licenseId);
        settings.setValue(KeygenConfig::SETTINGS_LICENSE_TYPE, "commercial");
        return true;
    }

    // Lizenz muss existieren — andere Fehler ablehnen
    if (licenseId.isEmpty() && validationCode != QLatin1String("NO_MACHINES")
                             && validationCode != QLatin1String("NO_MACHINE")
                             && validationCode != QLatin1String("FINGERPRINT_SCOPE_MISMATCH")) {
        m_lastError = tr("Ungültiger Lizenzschlüssel: %1").arg(validationCode);
        qCWarning(licenseLog) << "Aktivierung abgebrochen:" << m_lastError;
        return false;
    }

    // Lizenz-ID ohne vorherige Validierung holen (falls validationCode passt)
    // licenseId kann leer sein falls NOT_FOUND → Abbruch
    if (licenseId.isEmpty()) {
        m_lastError = tr("Lizenz nicht gefunden.");
        return false;
    }

    // --- Schritt 2: Maschine aktivieren ---
    // Authentifizierung: "License <key>" erlaubt Client-seitige Aktivierung
    // (kein Product Token nötig, wenn die Policy dies erlaubt)
    const QString authToken = QStringLiteral("License ") + licenseKey;

    QJsonObject machineAttrs;
    machineAttrs[QLatin1String("fingerprint")] = fingerprint;
    machineAttrs[QLatin1String("platform")]    = QSysInfo::prettyProductName();
    machineAttrs[QLatin1String("name")]        = QSysInfo::machineHostName();

    QJsonObject licenseRef;
    licenseRef[QLatin1String("type")] = QLatin1String("licenses");
    licenseRef[QLatin1String("id")]   = licenseId;

    QJsonObject licenseRel;
    licenseRel[QLatin1String("data")] = licenseRef;

    QJsonObject relationships;
    relationships[QLatin1String("license")] = licenseRel;

    QJsonObject data;
    data[QLatin1String("type")]          = QLatin1String("machines");
    data[QLatin1String("attributes")]    = machineAttrs;
    data[QLatin1String("relationships")] = relationships;

    QJsonObject payload;
    payload[QLatin1String("data")] = data;

    int httpStatus = 0;
    const QString endpoint = QStringLiteral("/machines");
    const QJsonDocument response = postJson(endpoint, payload, authToken, httpStatus);

    if (httpStatus == 201) {
        // Maschine erfolgreich aktiviert
        qCInfo(licenseLog) << "Maschine erfolgreich aktiviert. HTTP 201.";
        QSettings settings;
        settings.setValue(KeygenConfig::SETTINGS_LICENSE_KEY,  licenseKey);
        settings.setValue(KeygenConfig::SETTINGS_LICENSE_ID,   licenseId);
        settings.setValue(KeygenConfig::SETTINGS_LICENSE_TYPE, "commercial");
        return true;
    }

    if (httpStatus == 422) {
        // Maschine bereits aktiviert (Duplikat) — kein echter Fehler
        const QJsonArray errors = response.object()
                                          .value(QLatin1String("errors"))
                                          .toArray();
        if (!errors.isEmpty()) {
            const QString code = errors.first()
                                       .toObject()
                                       .value(QLatin1String("code"))
                                       .toString();
            if (code == QLatin1String("MACHINE_ALREADY_ACTIVATED")) {
                qCInfo(licenseLog) << "Maschine war bereits aktiviert — gilt als Erfolg.";
                QSettings settings;
                settings.setValue(KeygenConfig::SETTINGS_LICENSE_KEY,  licenseKey);
                settings.setValue(KeygenConfig::SETTINGS_LICENSE_ID,   licenseId);
                settings.setValue(KeygenConfig::SETTINGS_LICENSE_TYPE, "commercial");
                return true;
            }
            m_lastError = code;
        }
    }

    qCWarning(licenseLog) << "Maschinenaktivierung fehlgeschlagen. HTTP" << httpStatus;
    if (m_lastError.isEmpty()) {
        m_lastError = tr("Aktivierung fehlgeschlagen (HTTP %1)").arg(httpStatus);
    }
    return false;
}

// ---------------------------------------------------------------------------
// startTrial()
// Erstellt eine neue Trial-Lizenz über die Keygen.sh API und aktiviert sofort
// diese Maschine. Erfordert den KEYGEN_PRODUCT_TOKEN (sollte serverseitig
// abgewickelt werden — hier zur Vollständigkeit direkt implementiert).
// ---------------------------------------------------------------------------
bool LicenseManager::startTrial()
{
    qCInfo(licenseLog) << "Starte Trial-Lizenz-Erstellung...";

    const QString fingerprint = generateFingerprint();
    const QString authToken   = QStringLiteral("Bearer ")
                              + QLatin1String(KeygenConfig::KEYGEN_PRODUCT_TOKEN);

    // --- Schritt 1: Neue Trial-Lizenz erstellen ---
    QJsonObject policyRef;
    policyRef[QLatin1String("type")] = QLatin1String("policies");
    policyRef[QLatin1String("id")]   = QLatin1String(KeygenConfig::KEYGEN_TRIAL_POLICY_ID);

    QJsonObject policyRel;
    policyRel[QLatin1String("data")] = policyRef;

    QJsonObject relationships;
    relationships[QLatin1String("policy")] = policyRel;

    // Optionale Metadaten: Fingerabdruck für spätere Zuordnung speichern
    QJsonObject metadata;
    metadata[QLatin1String("initialFingerprint")] = fingerprint;

    QJsonObject licenseAttrs;
    licenseAttrs[QLatin1String("metadata")] = metadata;

    QJsonObject data;
    data[QLatin1String("type")]          = QLatin1String("licenses");
    data[QLatin1String("attributes")]    = licenseAttrs;
    data[QLatin1String("relationships")] = relationships;

    QJsonObject payload;
    payload[QLatin1String("data")] = data;

    int httpStatus = 0;
    const QJsonDocument licenseResponse = postJson(
        QStringLiteral("/licenses"),
        payload,
        authToken,
        httpStatus
    );

    if (httpStatus != 201) {
        const QJsonArray errors = licenseResponse.object()
                                                 .value(QLatin1String("errors"))
                                                 .toArray();
        if (!errors.isEmpty()) {
            m_lastError = errors.first()
                                .toObject()
                                .value(QLatin1String("detail"))
                                .toString();
        } else {
            m_lastError = tr("Trial-Lizenzerstellung fehlgeschlagen (HTTP %1)").arg(httpStatus);
        }
        qCWarning(licenseLog) << "Trial-Erstellung fehlgeschlagen:" << m_lastError;
        return false;
    }

    // Lizenzschlüssel und ID aus der Antwort extrahieren
    const QJsonObject licenseData   = licenseResponse.object()
                                                     .value(QLatin1String("data"))
                                                     .toObject();
    const QString licenseId         = licenseData.value(QLatin1String("id")).toString();
    const QJsonObject licenseAttrsR = licenseData.value(QLatin1String("attributes")).toObject();
    const QString licenseKey        = licenseAttrsR.value(QLatin1String("key")).toString();
    const QString expiryStr         = licenseAttrsR.value(QLatin1String("expiry")).toString();

    qCInfo(licenseLog) << "Trial-Lizenz erstellt. ID:" << licenseId
                       << "Ablauf:" << expiryStr;

    // --- Schritt 2: Diese Maschine für die Trial-Lizenz aktivieren ---
    // Für Trial-Aktivierung den Lizenzschlüssel als Auth verwenden
    const QString licenseAuth = QStringLiteral("License ") + licenseKey;

    QJsonObject machineAttrs;
    machineAttrs[QLatin1String("fingerprint")] = fingerprint;
    machineAttrs[QLatin1String("platform")]    = QSysInfo::prettyProductName();
    machineAttrs[QLatin1String("name")]        = QSysInfo::machineHostName();

    QJsonObject licenseRef2;
    licenseRef2[QLatin1String("type")] = QLatin1String("licenses");
    licenseRef2[QLatin1String("id")]   = licenseId;

    QJsonObject licenseRel2;
    licenseRel2[QLatin1String("data")] = licenseRef2;

    QJsonObject relationships2;
    relationships2[QLatin1String("license")] = licenseRel2;

    QJsonObject machineData;
    machineData[QLatin1String("type")]          = QLatin1String("machines");
    machineData[QLatin1String("attributes")]    = machineAttrs;
    machineData[QLatin1String("relationships")] = relationships2;

    QJsonObject machinePayload;
    machinePayload[QLatin1String("data")] = machineData;

    int machineHttpStatus = 0;
    const QJsonDocument machineResponse = postJson(
        QStringLiteral("/machines"),
        machinePayload,
        licenseAuth,
        machineHttpStatus
    );

    if (machineHttpStatus != 201 && machineHttpStatus != 200) {
        qCWarning(licenseLog) << "Trial-Maschinenaktivierung fehlgeschlagen. HTTP"
                              << machineHttpStatus;
        m_lastError = tr("Trial-Aktivierung der Maschine fehlgeschlagen (HTTP %1)")
                      .arg(machineHttpStatus);
        // Hinweis: Lizenz wurde erstellt, Aktivierung schlug fehl → ggf. aufräumen
        return false;
    }

    // Alle Daten lokal speichern
    QSettings settings;
    settings.setValue(KeygenConfig::SETTINGS_LICENSE_KEY,   licenseKey);
    settings.setValue(KeygenConfig::SETTINGS_LICENSE_ID,    licenseId);
    settings.setValue(KeygenConfig::SETTINGS_LICENSE_TYPE,  "trial");
    settings.setValue(KeygenConfig::SETTINGS_TRIAL_EXPIRY,  expiryStr);

    qCInfo(licenseLog) << "Trial erfolgreich gestartet. Ablauf:" << expiryStr;
    return true;
}

// ---------------------------------------------------------------------------
// trialDaysRemaining()
// Berechnet die verbleibenden Trial-Tage aus dem gespeicherten Ablaufdatum.
// ---------------------------------------------------------------------------
int LicenseManager::trialDaysRemaining()
{
    QSettings settings;
    const QString expiryStr = settings.value(KeygenConfig::SETTINGS_TRIAL_EXPIRY).toString();

    if (expiryStr.isEmpty()) {
        return 0;
    }

    const QDateTime expiry  = QDateTime::fromString(expiryStr, Qt::ISODate);
    const QDateTime now     = QDateTime::currentDateTimeUtc();

    if (!expiry.isValid() || expiry <= now) {
        return 0;
    }

    // Auf ganze Tage abrunden
    const qint64 secsLeft = now.secsTo(expiry);
    return static_cast<int>(secsLeft / 86400);
}

// ---------------------------------------------------------------------------
// validateOnStartup()
// Wird beim App-Start aufgerufen. Prüft den Lizenzstatus und emittiert
// licenseDialogRequired() falls eine Benutzeraktion nötig ist.
// Gibt true zurück wenn die App gestartet werden darf.
// ---------------------------------------------------------------------------
bool LicenseManager::validateOnStartup()
{
    qCInfo(licenseLog) << "Lizenzprüfung beim Start...";

    const LicenseStatus status = checkLicense();

    switch (status) {
    case LicenseStatus::VALID:
        qCInfo(licenseLog) << "Kommerzielle Lizenz gültig. App wird gestartet.";
        return true;

    case LicenseStatus::TRIAL_ACTIVE: {
        const int daysLeft = trialDaysRemaining();
        qCInfo(licenseLog) << "Trial aktiv." << daysLeft << "Tage verbleibend.";
        // App darf starten; UI kann Warnung anzeigen
        return true;
    }

    case LicenseStatus::TRIAL_EXPIRED:
        qCWarning(licenseLog) << "Trial abgelaufen. Lizenzdialog wird angezeigt.";
        emit licenseDialogRequired();
        return false;

    case LicenseStatus::EXPIRED:
        qCWarning(licenseLog) << "Kommerzielle Lizenz abgelaufen.";
        emit licenseDialogRequired();
        return false;

    case LicenseStatus::NOT_ACTIVATED:
        qCInfo(licenseLog) << "Keine Aktivierung gefunden. Lizenzdialog wird angezeigt.";
        emit licenseDialogRequired();
        return false;

    case LicenseStatus::ERROR:
    default:
        qCWarning(licenseLog) << "Lizenzfehler:" << m_lastError
                              << "— App wird trotzdem gestartet (Netzwerkfehler toleriert).";
        // Bei Netzwerkfehlern App nicht blockieren (Offline-Toleranz)
        return true;
    }
}

// ===========================================================================
// Private Hilfsmethoden
// ===========================================================================

// ---------------------------------------------------------------------------
// postJson() — Synchroner HTTP POST mit JSON-Body
// Baut die vollständige URL auf: KEYGEN_API_BASE + ACCOUNT_ID + endpoint
// ---------------------------------------------------------------------------
QJsonDocument LicenseManager::postJson(const QString& endpoint,
                                       const QJsonObject& payload,
                                       const QString& authToken,
                                       int& httpStatus)
{
    const QString url = QLatin1String(KeygenConfig::KEYGEN_API_BASE)
                      + QLatin1String(KeygenConfig::KEYGEN_ACCOUNT_ID)
                      + endpoint;

    QNetworkRequest request{QUrl{url}};
    request.setHeader(QNetworkRequest::ContentTypeHeader,
                      QLatin1String("application/vnd.api+json"));
    request.setRawHeader("Accept",        "application/vnd.api+json");
    request.setRawHeader("Authorization", authToken.toUtf8());
    request.setRawHeader("Keygen-Version","1.5"); // Immer aktuelle API-Version anfordern

    const QByteArray body = QJsonDocument(payload).toJson(QJsonDocument::Compact);

    // Synchron über QEventLoop warten
    QEventLoop loop;
    QNetworkReply* reply = m_networkManager->post(request, body);

    connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);

    // Timeout nach 15 Sekunden um Deadlock zu vermeiden
    QTimer::singleShot(15000, &loop, &QEventLoop::quit);
    loop.exec();

    if (reply->error() != QNetworkReply::NoError) {
        qCWarning(licenseLog) << "Netzwerkfehler (POST" << endpoint << "):"
                              << reply->errorString();
        m_lastError = reply->errorString();
        httpStatus  = -1;
        reply->deleteLater();
        return QJsonDocument{};
    }

    httpStatus = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    const QByteArray responseData = reply->readAll();
    reply->deleteLater();

    QJsonParseError parseError;
    const QJsonDocument doc = QJsonDocument::fromJson(responseData, &parseError);

    if (parseError.error != QJsonParseError::NoError) {
        qCWarning(licenseLog) << "JSON-Parsing-Fehler:" << parseError.errorString();
    }

    return doc;
}

// ---------------------------------------------------------------------------
// getJson() — Synchroner HTTP GET
// ---------------------------------------------------------------------------
QJsonDocument LicenseManager::getJson(const QString& endpoint,
                                      const QString& authToken,
                                      int& httpStatus)
{
    const QString url = QLatin1String(KeygenConfig::KEYGEN_API_BASE)
                      + QLatin1String(KeygenConfig::KEYGEN_ACCOUNT_ID)
                      + endpoint;

    QNetworkRequest request{QUrl{url}};
    request.setRawHeader("Accept",        "application/vnd.api+json");
    request.setRawHeader("Authorization", authToken.toUtf8());
    request.setRawHeader("Keygen-Version","1.5");

    QEventLoop loop;
    QNetworkReply* reply = m_networkManager->get(request);

    connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    QTimer::singleShot(15000, &loop, &QEventLoop::quit);
    loop.exec();

    if (reply->error() != QNetworkReply::NoError) {
        qCWarning(licenseLog) << "Netzwerkfehler (GET" << endpoint << "):"
                              << reply->errorString();
        m_lastError = reply->errorString();
        httpStatus  = -1;
        reply->deleteLater();
        return QJsonDocument{};
    }

    httpStatus = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    const QByteArray responseData = reply->readAll();
    reply->deleteLater();

    QJsonParseError parseError;
    return QJsonDocument::fromJson(responseData, &parseError);
}

// ---------------------------------------------------------------------------
// validateLicenseKey()
// Sendet POST /licenses/actions/validate-key mit Fingerabdruck-Scope.
// Gibt die Lizenz-ID zurück; isValid und validationCode werden gesetzt.
// ---------------------------------------------------------------------------
QString LicenseManager::validateLicenseKey(const QString& licenseKey,
                                            const QString& fingerprint,
                                            bool& isValid,
                                            QString& validationCode)
{
    isValid        = false;
    validationCode = QString();

    // Scope auf Fingerabdruck einschränken für node-locked Validierung
    QJsonObject scopeObj;
    scopeObj[QLatin1String("fingerprint")] = fingerprint;

    QJsonObject meta;
    meta[QLatin1String("key")]   = licenseKey;
    meta[QLatin1String("scope")] = scopeObj;

    QJsonObject payload;
    payload[QLatin1String("meta")] = meta;

    // validate-key benötigt keine Authentifizierung (public endpoint)
    int httpStatus = 0;
    const QJsonDocument response = postJson(
        QStringLiteral("/licenses/actions/validate-key"),
        payload,
        QString(), // Kein Auth-Token erforderlich
        httpStatus
    );

    if (httpStatus == -1 || response.isNull()) {
        validationCode = QLatin1String("NETWORK_ERROR");
        return QString();
    }

    const QJsonObject root     = response.object();
    const QJsonObject metaResp = root.value(QLatin1String("meta")).toObject();
    const QJsonObject dataObj  = root.value(QLatin1String("data")).toObject();

    isValid        = metaResp.value(QLatin1String("valid")).toBool(false);
    validationCode = metaResp.value(QLatin1String("code")).toString();

    // Lizenz-ID aus den Dateobjekt extrahieren
    const QString licenseId = dataObj.value(QLatin1String("id")).toString();

    qCDebug(licenseLog) << "validate-key Antwort:"
                        << "valid=" << isValid
                        << "code=" << validationCode
                        << "licenseId=" << licenseId;

    return licenseId;
}