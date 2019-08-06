#!/usr/bin/env python

# Assume python3

# Assume using ../../data/SimpleSkinning/skinning.gltf scene

import json
import urllib.request

url = "http://localhost:21264/v1"
method = "POST"
headers = {"Content-Type" : "application/json"}

joint_transforms = [ {"joint_id" : 1, "rotation_angle" : [0.0, 0.0, 45.0]} ]

params = { "joint_transforms" : joint_transforms }

d = {"jsonrpc" : "2.0",
     "method" : "update",
     "params" : params
    }

j = json.dumps(d).encode('utf-8')

request = urllib.request.Request(url, data=j, method=method, headers=headers)
with urllib.request.urlopen(request) as response:
    body = response.read().decode('utf-8')
    print(body)
