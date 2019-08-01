# JSON-RPC over HTTP

## Version

We use JSON-RPC 2.0

## API endpoint

```
/v1
```

## Supported message

### Node transform

T.B.W.

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
