package wal

import (
	"bytes"
	"encoding/binary"
	"encoding/hex"
	"math/rand"
	"os"
	"testing"

	"github.com/aadit-n3rdy/keel/memtable/tree"
	"github.com/aadit-n3rdy/keel/types"
)

func TestWalRandom(t *testing.T) {
	wal, err := NewWal("./")
	if err != nil {
		t.Fatalf("ERROR: while creating WAL %s", err.Error())
	}
	src := rand.New(rand.NewSource(123))

	iters := 10000

	reference := make(map[int][]byte)

	for i := 0; i < iters; i++ {
		newKey := make([]byte, 0, 4)
		newKey = binary.BigEndian.AppendUint32(newKey, uint32(i))
		newValue := binary.BigEndian.AppendUint32(nil, src.Uint32())
		reference[i] = newValue
		wal.WriteOp(&types.Operation{
			OpType: types.OperationWrite,
			Key:    newKey,
			Value:  newValue,
		})
		if i%10 == 0 {
			wal.WriteOp(&types.Operation{
				OpType: types.OperationDelete,
				Key:    newKey,
				Value:  []byte{},
			})
		}
		// tree.Set(newKey, types.Value{Deleted: false, Data: newValue})
	}

	wal.Commit()
	wal.Close()

	tree := tree.NewTree()

	err = WalToMemtab("./", tree)
	if err != nil {
		t.Fatalf("%s", err.Error())
	}
	defer os.Remove("./wal.keelwal")

	for i := 0; i < iters; i++ {
		newKey := make([]byte, 0, 4)
		newKey = binary.BigEndian.AppendUint32(newKey, uint32(i))

		tval, ok := tree.Get(newKey)
		if i%10 == 0 {
			if !tval.Deleted {
				t.Errorf("key %d expected delete", i)
			} else {
				t.Logf("delete successful")
			}
		} else {
			if !ok {
				t.Errorf("could not find key %d", i)
			} else if !bytes.Equal(tval.Data, reference[i]) {
				t.Errorf("key %d, expected value %s, actual value %s", i, hex.EncodeToString(reference[i]), hex.EncodeToString(tval.Data))
			} else {
				t.Logf("write successful")
			}
		}
	}

}
