/*
 *   Translations of the Bluetooth audio plugin (all languages of the Wii U system settings)
 *
 *   This program is free software: you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, either version 2 of the License, or
 *   (at your option) any later version.
 */

#include "i18n.h"

#include <coreinit/userconfig.h>

#include <stdio.h>
#include <string.h>

namespace i18n {

namespace {

int sConsole  = LANG_ENGLISH;
int sOverride = LANG_AUTO;

// one row per message, columns in the order of enum Language:
// ja, en, fr, de, it, es, zh-Hans, ko, nl, pt, ru, zh-Hant
const char *const sTable[MSG_COUNT][LANG_COUNT] = {
    // S_READY
    {"準備完了（未接続）", "Ready (not connected)", "Prêt (non connecté)", "Bereit (nicht verbunden)",
     "Pronto (non connesso)", "Listo (no conectado)", "就绪（未连接）", "준비됨 (연결 안 됨)",
     "Gereed (niet verbonden)", "Pronto (não ligado)", "Готово (не подключено)", "就緒（未連線）"},
    // S_NOT_ACTIVE
    {"Bloopairモジュールが無効です（30_bloopair.rpxがない？）", "Bloopair module not active (30_bloopair.rpx missing?)",
     "Module Bloopair inactif (30_bloopair.rpx manquant ?)", "Bloopair-Modul nicht aktiv (30_bloopair.rpx fehlt?)",
     "Modulo Bloopair non attivo (manca 30_bloopair.rpx?)", "Módulo Bloopair inactivo (¿falta 30_bloopair.rpx?)",
     "Bloopair模块未启用（缺少30_bloopair.rpx？）", "Bloopair 모듈 비활성 (30_bloopair.rpx 없음?)",
     "Bloopair-module niet actief (30_bloopair.rpx ontbreekt?)", "Módulo Bloopair inativo (falta 30_bloopair.rpx?)",
     "Модуль Bloopair не активен (нет 30_bloopair.rpx?)", "Bloopair模組未啟用（缺少30_bloopair.rpx？）"},
    // S_CODEC_BAD
    {"デバイスがこの音声形式に対応していません", "Audio format not supported by the device",
     "Format audio non pris en charge par l'appareil", "Audioformat vom Gerät nicht unterstützt",
     "Formato audio non supportato dal dispositivo", "Formato de audio no compatible con el dispositivo",
     "设备不支持该音频格式", "기기가 오디오 형식을 지원하지 않습니다", "Audioformaat niet ondersteund door het apparaat",
     "Formato de áudio não suportado pelo dispositivo", "Устройство не поддерживает аудиоформат", "裝置不支援此音訊格式"},
    // S_CONNECTED
    {"接続中: %s", "Connected: %s", "Connecté : %s", "Verbunden: %s", "Connesso: %s", "Conectado: %s", "已连接：%s",
     "연결됨: %s", "Verbonden: %s", "Ligado: %s", "Подключено: %s", "已連線：%s"},
    // S_PAIR_FAILED
    {"ペアリングに失敗しました（ペアリングモードですか？）", "Pairing failed (is the device in pairing mode?)",
     "Échec de l'appairage (appareil en mode appairage ?)", "Koppeln fehlgeschlagen (Gerät im Kopplungsmodus?)",
     "Associazione non riuscita (dispositivo in modalità associazione?)",
     "Error al emparejar (¿dispositivo en modo emparejamiento?)", "配对失败（设备是否处于配对模式？）",
     "페어링 실패 (기기가 페어링 모드인가요?)", "Koppelen mislukt (apparaat in koppelmodus?)",
     "Falha no emparelhamento (dispositivo em modo de emparelhamento?)",
     "Сопряжение не удалось (устройство в режиме сопряжения?)", "配對失敗（裝置是否處於配對模式？）"},
    // S_CONNECT_FAILED
    {"接続に失敗しました（電源は入っていますか？だめなら再ペアリング）",
     "Connection failed (device switched on? otherwise pair again)",
     "Échec de la connexion (appareil allumé ? sinon réappairer)",
     "Verbinden fehlgeschlagen (Gerät an? sonst neu koppeln)",
     "Connessione non riuscita (dispositivo acceso? altrimenti riassocia)",
     "Error de conexión (¿dispositivo encendido? si no, vuelve a emparejar)", "连接失败（设备已开启？否则请重新配对）",
     "연결 실패 (기기가 켜져 있나요? 아니면 다시 페어링)", "Verbinden mislukt (apparaat aan? anders opnieuw koppelen)",
     "Falha na ligação (dispositivo ligado? caso contrário, emparelhe de novo)",
     "Не удалось подключиться (устройство включено? иначе повторите сопряжение)", "連線失敗（裝置已開啟？否則請重新配對）"},
    // S_DISCONNECTED
    {"切断しました", "Disconnected", "Déconnecté", "Getrennt", "Disconnesso", "Desconectado", "已断开连接", "연결 해제됨",
     "Verbinding verbroken", "Desligado", "Отключено", "已中斷連線"},
    // S_CONNECTING
    {"%s に接続中...", "Connecting to %s ...", "Connexion à %s...", "Verbinde mit %s ...", "Connessione a %s...",
     "Conectando con %s...", "正在连接 %s...", "%s에 연결 중...", "Verbinden met %s...", "A ligar a %s...",
     "Подключение к %s...", "正在連線 %s..."},
    // S_PAIRING
    {"%s とペアリング中...", "Pairing with %s ...", "Appairage avec %s...", "Koppeln mit %s ...",
     "Associazione con %s...", "Emparejando con %s...", "正在与 %s 配对...", "%s와(과) 페어링 중...",
     "Koppelen met %s...", "A emparelhar com %s...", "Сопряжение с %s...", "正在與 %s 配對..."},
    // S_TIMEOUT
    {"接続がタイムアウトしました", "Connection timed out", "Délai de connexion dépassé", "Zeitüberschreitung beim Verbinden",
     "Timeout della connessione", "Tiempo de conexión agotado", "连接超时", "연결 시간 초과", "Time-out bij verbinden",
     "Tempo de ligação esgotado", "Время подключения истекло", "連線逾時"},
    // S_SCAN_BUSY
    {"検索する前に切断してください", "Disconnect first to search", "Déconnectez d'abord pour rechercher",
     "Zum Suchen zuerst trennen", "Disconnetti prima di cercare", "Desconecta primero para buscar", "请先断开连接再搜索",
     "검색하려면 먼저 연결을 해제하세요", "Verbreek eerst de verbinding om te zoeken", "Desligue primeiro para procurar",
     "Сначала отключитесь, чтобы начать поиск", "請先中斷連線再搜尋"},
    // S_SCANNING
    {"検索中（約20秒）... デバイスをペアリングモードに！", "Searching (about 20 s) ... put the device in pairing mode!",
     "Recherche (environ 20 s)... mettez l'appareil en mode appairage !",
     "Suche läuft (etwa 20 s) ... Gerät in den Kopplungsmodus!",
     "Ricerca (circa 20 s)... metti il dispositivo in modalità associazione!",
     "Buscando (unos 20 s)... ¡pon el dispositivo en modo emparejamiento!", "正在搜索（约20秒）... 请让设备进入配对模式！",
     "검색 중 (약 20초)... 기기를 페어링 모드로 전환하세요!", "Zoeken (ongeveer 20 s)... zet het apparaat in koppelmodus!",
     "A procurar (cerca de 20 s)... coloque o dispositivo em modo de emparelhamento!",
     "Поиск (около 20 с)... переведите устройство в режим сопряжения!", "正在搜尋（約20秒）... 請讓裝置進入配對模式！"},
    // S_SCAN_UNAVAILABLE
    {"検索できません", "Search not possible", "Recherche impossible", "Suche nicht möglich", "Ricerca non possibile",
     "No se puede buscar", "无法搜索", "검색할 수 없습니다", "Zoeken niet mogelijk", "Não é possível procurar",
     "Поиск невозможен", "無法搜尋"},
    // S_SCAN_NONE
    {"見つかりませんでした。もう一度検索してください", "Nothing found. Search again.",
     "Aucun appareil trouvé. Relancez la recherche.", "Nichts gefunden. Erneut suchen.",
     "Nessun dispositivo trovato. Cerca di nuovo.", "No se encontró nada. Vuelve a buscar.", "未找到设备。请重新搜索。",
     "찾은 기기가 없습니다. 다시 검색하세요.", "Niets gevonden. Zoek opnieuw.", "Nada encontrado. Procure novamente.",
     "Ничего не найдено. Повторите поиск.", "未找到裝置。請重新搜尋。"},
    // S_SCAN_DONE
    {"検索完了：「新しいデバイスをペアリング」で選択", "Search finished: choose it under \"Pair new device\"",
     "Recherche terminée : choisissez dans \"Appairer un appareil\"",
     "Suche beendet: unter \"Neues Gerät koppeln\" auswählen",
     "Ricerca terminata: scegli in \"Associa nuovo dispositivo\"",
     "Búsqueda terminada: elige en \"Emparejar dispositivo nuevo\"", "搜索完成：请在“配对新设备”中选择",
     "검색 완료: \"새 기기 페어링\"에서 선택하세요", "Zoeken voltooid: kies bij \"Nieuw apparaat koppelen\"",
     "Procura concluída: escolha em \"Emparelhar novo dispositivo\"",
     "Поиск завершён: выберите в \"Сопряжение нового устройства\"", "搜尋完成：請在「配對新裝置」中選擇"},
    // S_BUSY
    {"すでに接続中です。先に切断してください", "Already connected or busy - disconnect first",
     "Déjà connecté ou occupé - déconnectez d'abord", "Bereits verbunden oder beschäftigt - erst trennen",
     "Già connesso o occupato - disconnetti prima", "Ya conectado u ocupado - desconecta primero",
     "已连接或忙碌中 - 请先断开", "이미 연결되었거나 사용 중 - 먼저 연결 해제", "Al verbonden of bezig - verbreek eerst",
     "Já ligado ou ocupado - desligue primeiro", "Уже подключено или занято - сначала отключитесь", "已連線或忙碌中 - 請先中斷"},
    // S_DISCONNECTING
    {"切断中...", "Disconnecting ...", "Déconnexion...", "Trenne ...", "Disconnessione...", "Desconectando...",
     "正在断开...", "연결 해제 중...", "Verbinding verbreken...", "A desligar...", "Отключение...", "正在中斷..."},
    // S_RESET
    {"プラグインを初期設定に戻しました", "Plugin reset to defaults", "Plugin réinitialisé", "Plugin auf Standard zurückgesetzt",
     "Plugin ripristinato", "Plugin restablecido", "插件已恢复默认设置", "플러그인을 기본값으로 초기화했습니다",
     "Plug-in teruggezet naar standaard", "Plugin reposto para as predefinições", "Плагин сброшен к настройкам по умолчанию",
     "外掛程式已恢復預設設定"},
    // S_MODULE_REPLACED
    {"音声なしのBloopairが読み込まれています（更新？）", "Bloopair without audio loaded (update?)",
     "Bloopair sans audio chargé (mise à jour ?)", "Bloopair ohne Audio geladen (Update?)",
     "Bloopair senza audio caricato (aggiornamento?)", "Bloopair sin audio cargado (¿actualización?)",
     "已加载不含音频的Bloopair（更新？）", "오디오 없는 Bloopair가 로드됨 (업데이트?)",
     "Bloopair zonder audio geladen (update?)", "Bloopair sem áudio carregado (atualização?)",
     "Загружен Bloopair без звука (обновление?)", "已載入不含音訊的Bloopair（更新？）"},
    // S_RESTORED
    {"モジュールを復元しました。本体を再起動してください", "Module restored, please restart the console",
     "Module restauré, redémarrez la console", "Modul wiederhergestellt, bitte Konsole neu starten",
     "Modulo ripristinato, riavvia la console", "Módulo restaurado, reinicia la consola",
     "模块已恢复，请重新启动主机", "모듈을 복원했습니다. 본체를 다시 시작하세요",
     "Module hersteld, start de console opnieuw", "Módulo restaurado, reinicie a consola",
     "Модуль восстановлен, перезапустите консоль", "模組已恢復，請重新啟動主機"},
    // S_RESTORE_FAILED
    {"復元できませんでした（バックアップがありませんか？）", "Restore failed (backup missing?)",
     "Échec de la restauration (sauvegarde manquante ?)", "Wiederherstellen fehlgeschlagen (Sicherung fehlt?)",
     "Ripristino non riuscito (manca il backup?)", "Error al restaurar (¿falta la copia?)",
     "恢复失败（缺少备份？）", "복원 실패 (백업이 없나요?)", "Herstellen mislukt (back-up ontbreekt?)",
     "Falha ao restaurar (falta a cópia?)", "Не удалось восстановить (нет резервной копии?)", "恢復失敗（缺少備份？）"},
    // S_REMOVED
    {"%s を削除しました", "%s removed", "%s supprimé", "%s entfernt", "%s rimosso", "%s eliminado", "已删除 %s",
     "%s 삭제됨", "%s verwijderd", "%s removido", "%s удалено", "已刪除 %s"},
    // N_PREFIX
    {"Bluetoothオーディオ: ", "Bluetooth audio: ", "Audio Bluetooth : ", "Bluetooth-Audio: ", "Audio Bluetooth: ",
     "Audio Bluetooth: ", "蓝牙音频：", "블루투스 오디오: ", "Bluetooth-audio: ", "Áudio Bluetooth: ", "Bluetooth-аудио: ",
     "藍牙音訊："},
    // N_CONNECTED
    {"%s に接続しました", "Connected to %s", "Connecté à %s", "Verbunden mit %s", "Connesso a %s", "Conectado a %s",
     "已连接到 %s", "%s에 연결됨", "Verbonden met %s", "Ligado a %s", "Подключено к %s", "已連線到 %s"},
    // N_PAIRED
    {"%s をペアリングして保存しました", "%s paired and saved", "%s appairé et enregistré", "%s gekoppelt und gespeichert",
     "%s associato e salvato", "%s emparejado y guardado", "%s 已配对并保存", "%s 페어링 및 저장 완료",
     "%s gekoppeld en opgeslagen", "%s emparelhado e guardado", "%s: сопряжение выполнено и сохранено", "%s 已配對並儲存"},
    // N_SCAN_DONE
    {"検索完了：%d台見つかりました", "Search finished: %d device(s) found", "Recherche terminée : %d appareil(s) trouvé(s)",
     "Suche beendet: %d Gerät(e) gefunden", "Ricerca terminata: dispositivi trovati: %d",
     "Búsqueda terminada: %d dispositivo(s) encontrado(s)", "搜索完成：找到 %d 个设备", "검색 완료: 기기 %d개 발견",
     "Zoeken voltooid: %d apparaat/apparaten gevonden", "Procura concluída: %d dispositivo(s) encontrado(s)",
     "Поиск завершён: найдено устройств: %d", "搜尋完成：找到 %d 個裝置"},
    // N_SCAN_NONE
    {"検索完了：見つかりませんでした", "Search finished, nothing found", "Recherche terminée, aucun appareil trouvé",
     "Suche beendet, nichts gefunden", "Ricerca terminata, nessun dispositivo", "Búsqueda terminada, no se encontró nada",
     "搜索完成，未找到设备", "검색 완료, 찾은 기기 없음", "Zoeken voltooid, niets gevonden", "Procura concluída, nada encontrado",
     "Поиск завершён, ничего не найдено", "搜尋完成，未找到裝置"},
    // N_PAIR_FAILED
    {"ペアリングに失敗しました", "Pairing failed", "Échec de l'appairage", "Koppeln fehlgeschlagen", "Associazione non riuscita",
     "Error al emparejar", "配对失败", "페어링 실패", "Koppelen mislukt", "Falha no emparelhamento", "Сопряжение не удалось",
     "配對失敗"},
    // N_CONNECT_FAILED
    {"接続に失敗しました", "Connection failed", "Échec de la connexion", "Verbinden fehlgeschlagen", "Connessione non riuscita",
     "Error de conexión", "连接失败", "연결 실패", "Verbinden mislukt", "Falha na ligação", "Не удалось подключиться", "連線失敗"},

    // N_MODULE_REPLACED
    {"Bloopairが置き換えられ（更新？）、音声はオフです。プラグインメニューで復元できます",
     "Bloopair was replaced (update?), audio is off. Restore it in the plugin menu",
     "Bloopair a été remplacé (mise à jour ?), l'audio est désactivé. Restaurez-le dans le menu du plugin",
     "Bloopair wurde ersetzt (Update?), Audio ist aus. Im Plugin-Menü wiederherstellen",
     "Bloopair è stato sostituito (aggiornamento?), l'audio è spento. Ripristinalo nel menu del plugin",
     "Bloopair fue reemplazado (¿actualización?), el audio está apagado. Restáuralo en el menú del plugin",
     "Bloopair已被替换（更新？），音频已关闭。请在插件菜单中恢复",
     "Bloopair가 교체되어 (업데이트?) 오디오가 꺼졌습니다. 플러그인 메뉴에서 복원하세요",
     "Bloopair is vervangen (update?), audio staat uit. Herstel het in het plug-inmenu",
     "O Bloopair foi substituído (atualização?), o áudio está desligado. Restaure-o no menu do plugin",
     "Bloopair был заменён (обновление?), звук выключен. Восстановите его в меню плагина",
     "Bloopair已被替換（更新？），音訊已關閉。請在外掛程式選單中恢復"},
    // M_TITLE
    {"Bluetoothオーディオ", "Bluetooth audio", "Audio Bluetooth", "Bluetooth-Audio", "Audio Bluetooth", "Audio Bluetooth",
     "蓝牙音频", "블루투스 오디오", "Bluetooth-audio", "Áudio Bluetooth", "Bluetooth-аудио", "藍牙音訊"},
    // M_STATUS
    {"状態: %s", "Status: %s", "État : %s", "Status: %s", "Stato: %s", "Estado: %s", "状态：%s", "상태: %s", "Status: %s",
     "Estado: %s", "Состояние: %s", "狀態：%s"},
    // M_ON
    {"オン", "On", "Activé", "An", "Attivo", "Activado", "开", "켜기", "Aan", "Ativado", "Вкл.", "開"},
    // M_OFF
    {"オフ", "Off", "Désactivé", "Aus", "Disattivo", "Desactivado", "关", "끄기", "Uit", "Desativado", "Выкл.", "關"},
    // M_CONNECT
    {"接続する", "Connect to", "Se connecter à", "Verbinden mit", "Connetti a", "Conectar con", "连接到", "연결할 기기",
     "Verbinden met", "Ligar a", "Подключить", "連線到"},
    // M_NO_SAVED
    {"（保存されたデバイスなし）", "(no saved device)", "(aucun appareil enregistré)", "(kein Gerät gespeichert)",
     "(nessun dispositivo salvato)", "(ningún dispositivo guardado)", "（没有保存的设备）", "(저장된 기기 없음)",
     "(geen opgeslagen apparaat)", "(nenhum dispositivo guardado)", "(нет сохранённых устройств)", "（沒有儲存的裝置）"},
    // M_DISCONNECT
    {"切断", "Disconnect", "Déconnecter", "Trennen", "Disconnetti", "Desconectar", "断开连接", "연결 해제",
     "Verbinding verbreken", "Desligar", "Отключить", "中斷連線"},
    // M_PRESS_A
    {"Aボタンを押す", "press A", "appuyez sur A", "A drücken", "premi A", "pulsa A", "按A键", "A 버튼", "druk op A",
     "prima A", "нажмите A", "按A鍵"},
    // M_VOLUME
    {"音量（%）", "Volume (%)", "Volume (%)", "Lautstärke (%)", "Volume (%)", "Volumen (%)", "音量（%）", "볼륨 (%)",
     "Volume (%)", "Volume (%)", "Громкость (%)", "音量（%）"},
    // M_CAT_PAIR
    {"新しいデバイスをペアリング", "Pair new device", "Appairer un appareil", "Neues Gerät koppeln",
     "Associa nuovo dispositivo", "Emparejar dispositivo nuevo", "配对新设备", "새 기기 페어링", "Nieuw apparaat koppelen",
     "Emparelhar novo dispositivo", "Сопряжение нового устройства", "配對新裝置"},
    // M_STEP1
    {"1. デバイスをペアリングモードにする", "1. Put the device in pairing mode", "1. Mettez l'appareil en mode appairage",
     "1. Gerät in den Kopplungsmodus bringen", "1. Metti il dispositivo in modalità associazione",
     "1. Pon el dispositivo en modo emparejamiento", "1. 让设备进入配对模式", "1. 기기를 페어링 모드로 전환",
     "1. Zet het apparaat in koppelmodus", "1. Coloque o dispositivo em modo de emparelhamento",
     "1. Переведите устройство в режим сопряжения", "1. 讓裝置進入配對模式"},
    // M_SEARCH
    {"2. 検索開始（20秒）", "2. Start search (20 s)", "2. Lancer la recherche (20 s)", "2. Suche starten (20 s)",
     "2. Avvia ricerca (20 s)", "2. Iniciar búsqueda (20 s)", "2. 开始搜索（20秒）", "2. 검색 시작 (20초)",
     "2. Zoeken starten (20 s)", "2. Iniciar procura (20 s)", "2. Начать поиск (20 с)", "2. 開始搜尋（20秒）"},
    // M_STEP3
    {"3. メニューを閉じて待ち、もう一度開く", "3. Close the menu, wait, open it again", "3. Fermez le menu, attendez, rouvrez-le",
     "3. Menü schließen, warten, neu öffnen", "3. Chiudi il menu, attendi, riaprilo",
     "3. Cierra el menú, espera y vuelve a abrirlo", "3. 关闭菜单，等待后重新打开", "3. 메뉴를 닫고 기다린 후 다시 열기",
     "3. Sluit het menu, wacht, open het opnieuw", "3. Feche o menu, aguarde e abra-o novamente",
     "3. Закройте меню, подождите, откройте снова", "3. 關閉選單，等待後重新開啟"},
    // M_PAIR
    {"4. ペアリング", "4. Pair", "4. Appairer", "4. Koppeln", "4. Associa", "4. Emparejar", "4. 配对", "4. 페어링",
     "4. Koppelen", "4. Emparelhar", "4. Выполнить сопряжение", "4. 配對"},
    // M_NOTHING_FOUND
    {"4. （まだデバイスが見つかっていません）", "4. (no device found yet)", "4. (aucun appareil trouvé pour l'instant)",
     "4. (noch kein Gerät gefunden)", "4. (ancora nessun dispositivo trovato)", "4. (aún no se encontró ningún dispositivo)",
     "4. （尚未找到设备）", "4. (아직 찾은 기기 없음)", "4. (nog geen apparaat gevonden)",
     "4. (ainda nenhum dispositivo encontrado)", "4. (устройства пока не найдены)", "4. （尚未找到裝置）"},
    // M_CAT_SETTINGS
    {"設定", "Settings", "Paramètres", "Einstellungen", "Impostazioni", "Ajustes", "设置", "설정", "Instellingen",
     "Definições", "Настройки", "設定"},
    // M_SOURCE
    {"音声の取得元", "Sound source", "Source du son", "Tonquelle", "Sorgente audio", "Fuente de sonido", "声音来源",
     "사운드 소스", "Geluidsbron", "Fonte de som", "Источник звука", "聲音來源"},
    // M_SRC_TV
    {"テレビ", "TV", "Téléviseur", "TV", "TV", "TV", "电视", "TV", "Tv", "Televisor", "Телевизор", "電視"},
    // M_AUTO
    {"自動", "Automatic", "Automatique", "Automatisch", "Automatico", "Automático", "自动", "자동", "Automatisch",
     "Automático", "Автоматически", "自動"},
    // M_RESET
    {"プラグインをリセット（すべて削除）", "Reset plugin (deletes everything)", "Réinitialiser le plugin (tout supprimer)",
     "Plugin zurücksetzen (alles löschen)", "Reimposta plugin (elimina tutto)", "Restablecer plugin (borra todo)",
     "重置插件（全部删除）", "플러그인 초기화 (모두 삭제)", "Plug-in resetten (alles wissen)",
     "Repor plugin (apaga tudo)", "Сбросить плагин (удалить всё)", "重設外掛程式（全部刪除）"},
    // M_REMOVE
    {"削除", "Remove", "Supprimer", "Entfernen", "Rimuovi", "Quitar", "删除", "삭제", "Verwijderen", "Remover",
     "Удалить", "刪除"},
    // M_HOTKEY
    {"接続/切断のボタン（長押し）", "Connect / disconnect button combo (hold)", "Combinaison connecter / déconnecter (maintenir)",
     "Tastenkürzel Verbinden / Trennen (halten)", "Combinazione connetti / disconnetti (tieni premuto)",
     "Combinación conectar / desconectar (mantener)", "连接/断开组合键（按住）", "연결/해제 버튼 조합 (길게 누르기)",
     "Toetscombinatie verbinden / verbreken (ingedrukt houden)", "Combinação ligar / desligar (manter premido)",
     "Сочетание кнопок: подключить / отключить (удерживать)", "連線/中斷組合鍵（按住）"},
    // M_LOW_LATENCY
    {"低遅延（途切れやすくなります）", "Low latency (more dropouts possible)", "Faible latence (plus de coupures possibles)",
     "Geringe Verzögerung (mehr Aussetzer möglich)", "Bassa latenza (più interruzioni possibili)",
     "Baja latencia (posibles más cortes)", "低延迟（可能更多断音）", "저지연 (끊김이 늘 수 있음)",
     "Lage vertraging (meer haperingen mogelijk)", "Baixa latência (mais cortes possíveis)",
     "Низкая задержка (возможны пропадания)", "低延遲（可能更多斷音）"},
    // M_RESTORE
    {"BTAudioモジュールを復元", "Restore BTAudio module", "Restaurer le module BTAudio", "BTAudio-Modul wiederherstellen",
     "Ripristina modulo BTAudio", "Restaurar módulo BTAudio", "恢复BTAudio模块", "BTAudio 모듈 복원",
     "BTAudio-module herstellen", "Restaurar módulo BTAudio", "Восстановить модуль BTAudio", "恢復BTAudio模組"},
    // M_BATTERY
    {"ヘッドホンのバッテリー残量を表示（テスト）", "Show headphone battery (test)", "Afficher la batterie du casque (test)",
     "Akkustand der Kopfhörer anzeigen (Test)", "Mostra batteria delle cuffie (test)", "Mostrar batería de los auriculares (prueba)",
     "显示耳机电量（测试）", "헤드폰 배터리 표시 (테스트)", "Batterij van de koptelefoon tonen (test)",
     "Mostrar bateria dos auscultadores (teste)", "Показывать заряд наушников (тест)", "顯示耳機電量（測試）"},
    // M_LANGUAGE
    {"言語", "Language", "Langue", "Sprache", "Lingua", "Idioma", "语言", "언어", "Taal", "Idioma", "Язык", "語言"},
    // M_CAT_LOG
    {"ログ（問題の調査用）", "Log (for finding problems)", "Journal (pour trouver les problèmes)",
     "Log (zur Fehlersuche)", "Log (per trovare i problemi)", "Registro (para buscar problemas)", "日志（用于排查问题）",
     "로그 (문제 찾기용)", "Logboek (om problemen te vinden)", "Registo (para encontrar problemas)",
     "Журнал (для поиска проблем)", "日誌（用於排查問題）"},
    // M_LOG
    {"ログを書き込む", "Write log", "Écrire le journal", "Log schreiben", "Scrivi log", "Escribir registro", "写入日志",
     "로그 기록", "Logboek schrijven", "Escrever registo", "Вести журнал", "寫入日誌"},
    // M_LOG_CONNECTION
    {"接続", "Connection", "Connexion", "Verbindung", "Connessione", "Conexión", "连接", "연결", "Verbinding", "Ligação",
     "Подключение", "連線"},
    // M_LOG_SCAN
    {"デバイスの検索", "Device search", "Recherche d'appareils", "Gerätesuche", "Ricerca dispositivi",
     "Búsqueda de dispositivos", "设备搜索", "기기 검색", "Apparaten zoeken", "Pesquisa de dispositivos",
     "Поиск устройств", "裝置搜尋"},
    // M_LOG_MENU
    {"メニューの操作", "Menu actions", "Actions du menu", "Menü-Auswahl", "Azioni del menu", "Acciones del menú",
     "菜单操作", "메뉴 동작", "Menuacties", "Ações do menu", "Действия в меню", "選單操作"},
    // M_LOG_APP
    {"ソフトの切り替え", "Application changes", "Changements d'application", "App-Wechsel", "Cambi di applicazione",
     "Cambios de aplicación", "应用切换", "앱 전환", "App-wissels", "Mudanças de aplicação", "Смена приложений",
     "應用程式切換"},
    // M_LOG_STATS
    {"音声の統計", "Audio statistics", "Statistiques audio", "Audio-Statistik", "Statistiche audio",
     "Estadísticas de audio", "音频统计", "오디오 통계", "Audiostatistieken", "Estatísticas de áudio",
     "Статистика звука", "音訊統計"},
    // M_EVERY
    {"%d秒ごと", "every %d s", "toutes les %d s", "alle %d s", "ogni %d s", "cada %d s", "每 %d 秒", "%d초마다",
     "elke %d s", "a cada %d s", "каждые %d с", "每 %d 秒"},
    // M_LOG_FILE
    {"ファイル: SD:/wiiu/btaudio/plugin.txt", "File: SD:/wiiu/btaudio/plugin.txt",
     "Fichier : SD:/wiiu/btaudio/plugin.txt", "Datei: SD:/wiiu/btaudio/plugin.txt",
     "File: SD:/wiiu/btaudio/plugin.txt", "Archivo: SD:/wiiu/btaudio/plugin.txt", "文件：SD:/wiiu/btaudio/plugin.txt",
     "파일: SD:/wiiu/btaudio/plugin.txt", "Bestand: SD:/wiiu/btaudio/plugin.txt",
     "Ficheiro: SD:/wiiu/btaudio/plugin.txt", "Файл: SD:/wiiu/btaudio/plugin.txt", "檔案：SD:/wiiu/btaudio/plugin.txt"},
    // M_QUALITY
    {"音質（次の接続から）", "Audio quality (from next connection)", "Qualité audio (à la prochaine connexion)",
     "Audioqualität (ab nächster Verbindung)", "Qualità audio (dalla prossima connessione)",
     "Calidad de audio (desde la próxima conexión)", "音质（下次连接时生效）", "음질 (다음 연결부터)",
     "Audiokwaliteit (vanaf volgende verbinding)", "Qualidade de áudio (a partir da próxima ligação)",
     "Качество звука (со следующего подключения)", "音質（下次連線時生效）"},
    // M_Q_HIGH
    {"高", "High", "Haute", "Hoch", "Alta", "Alta", "高", "높음", "Hoog", "Alta", "Высокое", "高"},
    // M_Q_MEDIUM
    {"中", "Medium", "Moyenne", "Mittel", "Media", "Media", "中", "중간", "Gemiddeld", "Média", "Среднее", "中"},
    // M_Q_STABLE
    {"安定（遠くても途切れにくい）", "Stable (fewer dropouts at a distance)", "Stable (moins de coupures à distance)",
     "Stabil (weniger Aussetzer auf Abstand)", "Stabile (meno interruzioni a distanza)",
     "Estable (menos cortes a distancia)", "稳定（远距离更少断音）", "안정 (멀리서도 끊김 적음)",
     "Stabiel (minder haperingen op afstand)", "Estável (menos cortes à distância)",
     "Стабильное (меньше пропаданий на расстоянии)", "穩定（遠距離較少斷音）"},
};

const char *const sLanguageNames[LANG_COUNT] = {
    "日本語", "English", "Français", "Deutsch", "Italiano", "Español",
    "简体中文", "한국어", "Nederlands", "Português", "Русский", "繁體中文",
};

} // namespace

void init() {
    UCHandle handle = UCOpen();
    if (handle < 0) {
        return;
    }

    uint32_t value = LANG_ENGLISH;
    UCSysConfig setting;
    memset(&setting, 0, sizeof(setting));
    strncpy(setting.name, "cafe.language", sizeof(setting.name) - 1);
    setting.access   = 0x777;
    setting.dataType = UC_DATATYPE_UNSIGNED_INT;
    setting.dataSize = sizeof(value);
    setting.data     = &value;
    if (UCReadSysConfig(handle, 1, &setting) == UC_ERROR_OK && value < LANG_COUNT) {
        sConsole = (int) value;
    }
    UCClose(handle);
}

void setOverride(int language) {
    sOverride = (language >= 0 && language < LANG_COUNT) ? language : LANG_AUTO;
}

int getOverride() {
    return sOverride;
}

int current() {
    return sOverride == LANG_AUTO ? sConsole : sOverride;
}

const char *tr(Msg msg) {
    if (msg < 0 || msg >= MSG_COUNT) {
        return "";
    }
    const char *text = sTable[msg][current()];
    if (!text) {
        text = sTable[msg][LANG_ENGLISH];
    }
    return text ? text : "";
}

std::string format(Msg msg, const char *text) {
    char buffer[256];
    snprintf(buffer, sizeof(buffer), tr(msg), text ? text : "");
    return buffer;
}

std::string format(Msg msg, int number) {
    char buffer[256];
    snprintf(buffer, sizeof(buffer), tr(msg), number);
    return buffer;
}

std::string formatEnglish(Msg msg, const char *text) {
    if (msg < 0 || msg >= MSG_COUNT || !sTable[msg][LANG_ENGLISH]) {
        return "";
    }
    char buffer[256];
    snprintf(buffer, sizeof(buffer), sTable[msg][LANG_ENGLISH], text ? text : "");
    return buffer;
}

const char *languageName(int language) {
    return (language >= 0 && language < LANG_COUNT) ? sLanguageNames[language] : "";
}

} // namespace i18n
