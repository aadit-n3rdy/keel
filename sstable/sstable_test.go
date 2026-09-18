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
			t.Fatalf("could not fetch key %d immediately after writing, missing in tree", i)
		} else if !bytes.Equal(testVal.Data, val) {
			t.Fatalf("did not fetch key %d immediately after writing, wrong value", i)
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
	t.Logf("Tree works fine")

	err := NewSSTableFile("./", "test", tree)
	if err != nil {
		t.Fatalf("error creating SSTab: %s", err.Error())
	}
	t.Logf("Wrote SSTable and sparse index")

	sstab, err := OpenSSTable("./", "test")
	if err != nil {
		t.Fatalf("error loading SSTab: %s", err.Error())
	}
	t.Logf("Loaded SSTable and sparse index with %v entries", len(sstab.sparseIndex))

	defer os.Remove("./test.keel.sstable")
	defer os.Remove("./test.keel.spindex")
	for i := range test_count {
		binary.Encode(keyBuf[:], binary.BigEndian, int32(i))

		key := genKey(keyBuf[:])
		val := genVal(key)

		result, err := sstab.Get(key)
		if err != nil {
			t.Errorf("sstab could not find key for i %d: error %s", i, err.Error())
		}
		res := result.Data
		if len(res) != len(val) {
			t.Errorf("sstab read back length mismatch: %d, expected %d", len(res), len(val))
		}
		if !bytes.Equal(res, val) {
			t.Errorf("sstab did not store right value for key %d", i)
		}
	}

}
