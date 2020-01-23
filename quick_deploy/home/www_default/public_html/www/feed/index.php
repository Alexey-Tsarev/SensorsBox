<?php
require_once 'lib.inc.php';
require_once 'zabbix-sender/src/Zabbix/Sender.php';
ignore_user_abort(true);

// Config
$DEBUG = false;
//$DEBUG = true;

$ZABBIX_HOST = '127.0.0.1';
$ZABBIX_RPC_URL = 'http://' . $ZABBIX_HOST . '/zabbix/api_jsonrpc.php';
$ZABBIX_USER = 'Admin';
$ZABBIX_PASSWORD = 'zabbix';
$SLEEP_AFTER_HOST_ADDED_SECONDS = 40;

$ITEMS_RESERVED = array();
$ITEMS_RESERVED[] = 'device';
$ITEMS_RESERVED[] = 'id';

$ITEMS_TYPE['Flag'] = 3; // 3 - numeric unsigned
$ITEMS_TYPE['Str'] = 4; // 4 - text
$ITEMS_TYPE[''] = 0; // 0 - numeric float
// End Config

// Zabbix login
$zbx = new zbx;
$zbx->url = $ZABBIX_RPC_URL;
$zbx->method = 'user.login';
$zbx->query['user'] = $ZABBIX_USER;
$zbx->query['password'] = $ZABBIX_PASSWORD;
$zbx->access_token = $zbx->call()['result'];

if (strlen($zbx->access_token) == 0)
    die("Failed to login to Zabbix: $ZABBIX_RPC_URL\n");
// End Zabbix login

$zbx_sender = new \Disc\Zabbix\Sender($ZABBIX_HOST);

//$device_raw_data = '{"device":"SensorsBox","id":"8800871","ip":"192.168.3.97","wifiStr":"ArtLuch.RU-IoT","uptime":"1010.062","freeHeap":17952,"heapFragmentation":2,"maxFreeBlockSize":17664,"z19Status":1,"co2":953,"co2Min":300,"co2Max":900,"co2AlarmFlag":1,"bmeStatus":2,"temperature":25.96,"temperatureMin":19,"temperatureMax":31,"temperatureAlarmFlag":0,"humidity":39.9873,"humidityMin":20,"humidityMax":90,"humidityAlarmFlag":0,"pressure":757.938,"pressureMin":700,"pressureMax":800,"pressureAlarmFlag":0}';
$device_raw_data = file_get_contents('php://input');

if (strlen($device_raw_data) == 0)
    die("No input data\n");

$device_json = json_decode($device_raw_data, true);

if ($DEBUG) {
    echo "device_json:\n";
    print_r($device_json);
    echo "\n";
}

$zbx_host = $device_json['device'] . '-' . $device_json['id'];

if (send_metrics($zbx_host, $device_json)) {
    echo "OK\n";
} else {
    echo "Create structures\n";

    // Group
    $zbx_group = $device_json['device'];
    $zbx_group_id = create_zbx_group_if_not_exists($zbx_group);
    if ($zbx_group_id === false)
        die("Failed to create group: $zbx_group\n");

    if ($DEBUG)
        echo "zbx_group_id: " . $zbx_group_id . "\n";
    // End Group

    // Host
    $zbx_host_ip = $device_json['ipStr'];
    $zbx_host_id = create_zbx_host_if_not_exists($zbx_host, array($zbx_group_id), $zbx_host_ip);
    if ($zbx_host_id === false)
        die("Failed to create host: $zbx_host\n");

    if ($DEBUG)
        echo "zbx_host_id: " . $zbx_host_id . "\n";
    // End Host

    // Items
    foreach ($device_json as $item => $value)
        if (in_array($item, $ITEMS_RESERVED) === false) {
            $zbx_item_id = create_zbx_item_if_not_exists($zbx_host_id, $item);

            if ($zbx_item_id === false)
                die("Failed to create item: $item\n");

            if ($DEBUG)
                echo "zbx_item_id: " . $zbx_item_id . "\n";
        }
    // End Items

    if ($DEBUG)
        echo "Sleep: $SLEEP_AFTER_HOST_ADDED_SECONDS\n";

    set_time_limit($SLEEP_AFTER_HOST_ADDED_SECONDS + 10);
    sleep($SLEEP_AFTER_HOST_ADDED_SECONDS);

    if (send_metrics_one_by_one($zbx_host, $device_json))
        echo "OK\n";
    else
        echo "ER\n";
}
