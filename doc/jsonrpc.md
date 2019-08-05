# JSON-RPC over HTTP

## Version

We use JSON-RPC 2.0

## API endpoint

```
/v1
```

## Supported message

### Joint transform

```
"joint_transforms" : ARRAY
```

Each object in ARRAY specifies the node translation/rotation/scale.

```json
{ "joint_id" : 0, // mandatory
  "translation": float[3], // optional
  "rotation": float[4], // optional
  "rotation_angle": float[3], // optional
  "scale": float[3] // optional
}
```

"rotation" parameter is specified by quaternion as in glTF spec.
"rotation_angle" parameter is a gltf-insight extension. It specifis rotation by Euler angle(in degree). The order of rotation axes is currently set to `XYZ`(Maya default).

Example:

```json
{ "jsonrpc" : "2.0",
  "method" : "update",
  "params" : {
    "joint_transforms" : [
      { "joint_id" : 0, "translation": [0.2, 0.4, 0.5] },
      { "joint_id" : 0, "rotation": [0.1, 0.1, 0.1, 0.9], "scale": [2, 2, 2] },
      { "joint_id" : 1, "rotation_angle": [30, 20, 40] }
    ]
  }
}
```

#### TODO

Update by matrix.

### Timeline

```
"timeline": { "current_time" : float }
```

Specify current playback time in seconds.


```
"timeline": { "current_frame" : int }
```

Specify current frame to display. frame is divided by FPS internally to calculate current time.

```
"timeline": { "fps" : int }
```

Set FPS.


Example:

```json
{ "jsonrpc" : "2.0",
  "method" : "update",
  "params" : {
    "timeline" : { "current_frame" : 12 }
    ]
  }
}
```

```json
{ "jsonrpc" : "2.0",
  "method" : "update",
  "params" : {
    "timeline" : { "current_time" : 0.75 }
    ]
  }
}
```

```json
{ "jsonrpc" : "2.0",
  "method" : "update",
  "params" : {
    "timeline" : { "fps" : 25 }
    ]
  }
}
```

### Bone transform

T.B.W.

### Morph target

```
"morph_weights" : ARRAY
```

Each object in ARRAY specifies the weight of each morph target.

```json
{ "target_id" : 0, "weight": 0.5 }
```

Example:

```json
{ "jsonrpc" : "2.0",
  "method" : "update",
  "params" : {
    "morph_weights" : [
      { "target_id" : 0, "weight": 0.5 },
      { "target_id" : 1, "weight": 0.0 }
    ]
  }
}
```

#### TODO

Specify target by name https://github.com/KhronosGroup/glTF/issues/1036
