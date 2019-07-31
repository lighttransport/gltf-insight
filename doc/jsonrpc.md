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

Specify the weight of each morph target.

```json
{ "target_id" : 0, "weight": 0.5 }
```

And Use `"morph_weight" : ARRAY` as an message tag.

Example:

```json
"params" : {
  "morph_weight" : [
    { "target_id" : 0, "weight": 0.5 },
    { "target_id" : 1, "weight": 0.0 }
  ]

```

#### TODO

Specify target by name https://github.com/KhronosGroup/glTF/issues/1036
