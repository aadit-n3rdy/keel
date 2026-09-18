package memtable

import "github.com/aadit-n3rdy/keel/types"

type Memtable interface {
	Get(key []byte) (types.Value, bool)     // returns result if exists, and bool if not found
	Set(key []byte, value types.Value) bool // returns true if exceeded size limit
	ForEach(func([]byte, types.Value) error) error
}
