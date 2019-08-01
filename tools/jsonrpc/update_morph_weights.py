#!/usr/bin/env python

# Assume python3

import json
import urllib.request

url = "http://localhost:21264/v1"
method = "POST"
headers = {"Content-Type" : "application/json"}

morph_weights = [ {"target_id" : 0, "weight" : 0.5} ]

params = { "morph_weights" : morph_weights }

d = {"jsonrpc" : "2.0",
     "method" : "update",
     "params" : params
    }

j = json.dumps(d).encode('utf-8')

request = urllib.request.Request(url, data=j, method=method, headers=headers)
with urllib.request.urlopen(request) as response:
    body = response.read().decode('utf-8')
    print(body)
