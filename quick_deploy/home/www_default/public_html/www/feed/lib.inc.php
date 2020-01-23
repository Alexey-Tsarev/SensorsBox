<?php

// Based on https://github.com/lundgrenalex/microzabbixapiconnector
class jsonrpc {
    protected function connect($server, $query) {
        $http = curl_init($server);
        curl_setopt($http, CURLOPT_CUSTOMREQUEST, 'POST');
        curl_setopt($http, CURLOPT_POSTFIELDS, $query);
        curl_setopt($http, CURLOPT_RETURNTRANSFER, TRUE);
        curl_setopt($http, CURLOPT_SSL_VERIFYPEER, FALSE);
        curl_setopt($http, CURLOPT_SSL_VERIFYHOST, FALSE);
//        curl_setopt($http, CURLOPT_PROXY, 'proxy_url');
//        curl_setopt($http, CURLOPT_PROXYPORT, '3128');
//        curl_setopt($http, CURLOPT_PROXYUSERPWD, 'login:pass');
        curl_setopt($http, CURLOPT_HTTPHEADER, ['Content-Type: application/json']);
        $response = curl_exec($http);
        curl_close($http);

        return json_decode($response, true);
    }
}

class zbx extends jsonrpc {
    public $method;
    public $access_token;
    public $url;
    public $query;

    function call() {
        $data['jsonrpc'] = '2.0';
        $data['method'] = $this->method;
        $data['params'] = $this->query;

        unset($this->query);

        if (!empty($this->access_token))
            $data['auth'] = $this->access_token;

        $data['id'] = rand(1, 100);
        $data = json_encode($data, JSON_PRETTY_PRINT);
        return $this->connect($this->url, $data);
    }
}

// End Based on


// Functions
function starts_with($haystack, $needle) {
    $length = strlen($needle);
    return (substr($haystack, 0, $length) === $needle);
}


function ends_with($haystack, $needle) {
    $length = strlen($needle);
    if ($length == 0) {
        return true;
    }

    return (substr($haystack, -$length) === $needle);
}


function create_zbx_group_if_not_exists($group) {
    global $zbx, $DEBUG;
    $zbx->method = 'hostgroup.get';
    $zbx->query['filter']['name'] = $group;

    if ($DEBUG)
        print_r($zbx);

    $res = $zbx->call();

    if ($DEBUG) {
        echo "$zbx->method:\n";
        print_r($res);
        echo "\n";
    }

    if (isset($res['result'][0]))
        for ($i = 0; $i < count($res['result']); $i++) {
            if ($group == $res['result'][$i]['name'])
                return $res['result'][$i]['groupid'];
        }

    // Create group
    if ($DEBUG)
        echo "Create group: " . $group . "\n";

    $zbx->method = 'hostgroup.create';
    $zbx->query['name'] = $group;

    if ($DEBUG)
        print_r($zbx);

    $res = $zbx->call();

    if ($DEBUG) {
        echo "$zbx->method:\n";
        print_r($res);
        echo "\n";
    }

    if (isset($res['result']['groupids']))
        return $res['result']['groupids'][0];
    else
        return false;
    // End Create group
}


function create_zbx_host_if_not_exists($host, $groups_id, $host_ip) {
    global $zbx, $DEBUG;
    $zbx->method = 'host.get';
    $zbx->query['filter']['host'] = $host;

    if ($DEBUG)
        print_r($zbx);

    $res = $zbx->call();

    if ($DEBUG) {
        echo "$zbx->method:\n";
        print_r($res);
        echo "\n";
    }

    if (isset($res['result'][0]))
        for ($i = 0; $i < count($res['result']); $i++) {
            if ($host == $res['result'][$i]['host'])
                return $res['result'][$i]['hostid'];
        }

    // Create host
    if ($DEBUG)
        echo "Create host: " . $host . "\n";

    $zbx->method = 'host.create';
    $zbx->query['host'] = $host;
    $zbx->query['interfaces'][0]['type'] = 1;
    $zbx->query['interfaces'][0]['main'] = 1;
    $zbx->query['interfaces'][0]['useip'] = 1;
    $zbx->query['interfaces'][0]['ip'] = $host_ip;
    $zbx->query['interfaces'][0]['dns'] = "";
    $zbx->query['interfaces'][0]['port'] = 10050;

    foreach ($groups_id as $group_id)
        $zbx->query['groups'][0]['groupid'] = $group_id;

    if ($DEBUG)
        print_r($zbx);

    $res = $zbx->call();

    if ($DEBUG) {
        echo "$zbx->method:\n";
        print_r($res);
        echo "\n";
    }

    if (isset($res['result']['hostids']))
        return $res['result']['hostids'][0];
    else
        return false;
    // End Create host
}


function create_zbx_item_if_not_exists($host_id, $item) {
    global $zbx, $DEBUG, $ITEMS_TYPE;
    $zbx->method = 'item.get';
    $zbx->query['hostids'] = $host_id;
    $zbx->query['search']['key_'] = $item;

    if ($DEBUG)
        print_r($zbx);

    $res = $zbx->call();

    if ($DEBUG) {
        echo "$zbx->method:\n";
        print_r($res);
        echo "\n";
    }

    if (isset($res['result'][0]))
        for ($i = 0; $i < count($res['result']); $i++) {
            if ($item == $res['result'][$i]['key_'])
                return $res['result'][$i]['itemid'];
        }

    // Create item
    if ($DEBUG)
        echo "Create item: " . $item . "\n";

    $zbx->method = 'item.create';
    $zbx->query['hostid'] = $host_id;
    $zbx->query['name'] = $item;
    $zbx->query['key_'] = $item;
    $zbx->query['type'] = 2; // 2 - Zabbix trapper
    $zbx->query['delay'] = '0';

    foreach ($ITEMS_TYPE as $item_ends_with => $item_type) {
        if (ends_with($item, $item_ends_with)) {
            $zbx->query['value_type'] = $item_type;
            break;
        }
    }

    if ($DEBUG)
        print_r($zbx);

    $res = $zbx->call();

    if ($DEBUG) {
        echo "$zbx->method:\n";
        print_r($res);
        echo "\n";
    }

    if (isset($res['result']['itemids']))
        return $res['result']['itemids'][0];
    else
        return false;
    // End Create item
}


function is_zbx_response_ok($zbx_response) {
    global $DEBUG;

    if ($DEBUG) {
        echo "Got response from Zabbix:\n";
        print_r($zbx_response);
        echo "\n";
    }

    if ((isset($zbx_response['response']) && ($zbx_response['response'] === 'success'))) {
        if (isset($zbx_response['info'])) {
            $zbx_response_info = $zbx_response['info'];

            $matches = [];
            $matched = preg_match('/\w+: (\d+); \w+: (\d+); \w+: (\d+); [a-z ]+: (\d+\.\d+)/',
                $zbx_response_info,
                $matches);

            $failed_items = -1;

            if (($matched !== false) && ($matched !== 0)) {
                $processed_items = intval($matches[1]);
                $failed_items = intval($matches[2]);
                $total_items = intval($matches[3]);
                $seconds_spent = floatval($matches[4]);
            }

            if ($failed_items == 0)
                return true;
        }
    }

    return false;
}


function send_metric($host, $item, $value) {
    global $zbx_sender, $DEBUG;
    $zbx_sender->addData($host, $item, $value);
    $zbx_sender->send();
    $zbx_response = $zbx_sender->getResponse();

    return is_zbx_response_ok($zbx_response);

}


function send_metrics_one_by_one($host, $metrics) {
    global $DEBUG, $ITEMS_RESERVED;

    foreach ($metrics as $metric => $value) {
        if (in_array($metric, $ITEMS_RESERVED) === false) {
            if ($DEBUG)
                echo "Send metric with value: $metric: $value\n";

            if (send_metric($host, $metric, $value) === false)
                return false;
        }
    }

    return true;
}


function send_metrics($host, $metrics) {
    global $zbx_sender, $DEBUG, $ITEMS_RESERVED;

    foreach ($metrics as $metric => $value)
        if (in_array($metric, $ITEMS_RESERVED) === false) {
            if ($DEBUG)
                echo "Add metric with value: $metric: $value\n";

            $zbx_sender->addData($host, $metric, $value);
        }

    if ($DEBUG)
        echo "\n";

    $zbx_sender->send();
    $zbx_response = $zbx_sender->getResponse();

    return is_zbx_response_ok($zbx_response);
}
