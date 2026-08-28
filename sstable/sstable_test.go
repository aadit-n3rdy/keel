package sstable

import (
	"bytes"
	"crypto/sha256"
	"encoding/binary"
	"os"
	"testing"

	"github.com/aadit-n3rdy/keel/tree"
	"github.com/aadit-n3rdy/keel/types"
)

func genKey(buf []byte) []byte {
	res := sha256.Sum256(buf)
	return res[:4]
}

func genVal(buf []byte) []byte {
	res := sha256.Sum256(buf)
	return res[:4]
}

func TestSSTable(t *testing.T) {
	tree := tree.NewTree()
	test_count := 3000
	keyBuf := [4]byte{}
	for i := 0; i < test_count; i++ {
		binary.Encode(keyBuf[:], binary.BigEndian, int32(i))

		key := genKey(keyBuf[:])
		val := genVal(key)

		tree.Set(key, types.Value{Deleted: false, Data: val})

		testVal, ok := tree.Get(key)
		if !ok {
			t.Errorf("could not fetch key %d immediately after writing, missing in tree", i)
		} else if !bytes.Equal(testVal.Data, val) {
			t.Errorf("did not fetch key %d immediately after writing, wrong value", i)
		}
	}

	// test if tree works right
	for i := 0; i < test_count; i++ {
		binary.Encode(keyBuf[:], binary.BigEndian, int32(i))

		key := genKey(keyBuf[:])
		val := genVal(key)

		res, ok := tree.Get(key)
		if !ok {
			t.Errorf("tree could not find key for i %d", i)
		}
		if !bytes.Equal(res.Data, val) {
			t.Errorf("tree did not store right value for key %d", i)
		}
	}

	sstab, err := NewSSTableFile("./", "test", tree)
	if err != nil {
		t.Errorf("error creating SSTab: %s", err.Error())
	}
	for i := 0; i < test_count; i++ {
		binary.Encode(keyBuf[:], binary.BigEndian, int32(i))

		key := genKey(keyBuf[:])
		val := genVal(key)

		res, err := sstab.Get(key)
		if err != nil {
			t.Errorf("sstab could not find key for i %d: error %s", i, err.Error())
		}
		if !bytes.Equal(res, val) {
			t.Errorf("sstab did not store right value for key %d", i)
		}
	}

	os.Remove("./test.sstable")
}
