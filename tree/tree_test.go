package tree

import (
	"bytes"
	"encoding/binary"
	"encoding/hex"
	"math/rand"
	"testing"

	"github.com/aadit-n3rdy/keel/types"
)

func TestTreeRandom(t *testing.T) {
	tree := NewTree()
	src := rand.New(rand.NewSource(123))

	iters := 1000

	reference := make(map[int][]byte)

	for i := 0; i < iters; i++ {
		newKey := make([]byte, 0, 4)
		newKey = binary.BigEndian.AppendUint32(newKey, uint32(i))
		newValue := binary.BigEndian.AppendUint32(nil, src.Uint32())
		reference[i] = newValue
		tree.Set(newKey, types.Value{Deleted: false, Data: newValue})
	}

	for i := 0; i < iters; i++ {
		newKey := make([]byte, 0, 4)
		newKey = binary.BigEndian.AppendUint32(newKey, uint32(i))

		tval, ok := tree.Get(newKey)
		if !ok {
			t.Errorf("could not find key %d", i)
		}
		if !bytes.Equal(tval.Data, reference[i]) {
			t.Errorf("key %d, expected value %s, actual value %s", i, hex.EncodeToString(reference[i]), hex.EncodeToString(tval.Data))
		}
	}
}
