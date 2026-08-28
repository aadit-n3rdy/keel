package sstable

// SSTable
// Each SSTable consists of a single SSTable file and a corresponding sparse index
// *id*.sst file: contains the actual data
// The file consists of a sequence of key value entries.
// Key: 4 byte length (big-endian), key in bytes
// Value: 4 byte length (big-endian), value in bytes

import (
	"bytes"
	"encoding/binary"
	"encoding/hex"
	"fmt"
	"os"
	"path"

	"github.com/aadit-n3rdy/keel/tree"
	"github.com/aadit-n3rdy/keel/types"
	"github.com/aadit-n3rdy/keel/util"
)

type SSTable struct {
	// manage handling an open SSTable file
	sparseIndex []sstIndexEntry
	f           *os.File
}

type sstIndexEntry struct {
	key    []byte
	offset int64
}

func NewSSTableFile(dir string, id string, memtab *tree.Tree) (*SSTable, error) {
	// create a new SSTable with the given memtab
	f, err := os.Create(path.Join(dir, id+".sstable"))
	if err != nil {
		return nil, err
	}

	err = memtab.ForEach(func(key []byte, value types.Value) error {
		err := util.WriteVarLenBytes(f, key)
		if err != nil {
			return err
		}
		buf, err := binary.Append(nil, binary.BigEndian, value)
		if err != nil {
			return err
		}
		err = util.WriteVarLenBytes(f, buf)
		if err != nil {
			return err
		}
		// fmt.Printf("Wrote to SSTable for key %s of value length %d\n", hex.EncodeToString(key), len(buf))
		return nil
	})
	f.Close()
	if err != nil {
		return nil, err
	}

	return OpenSSTable(dir, id)
}

func OpenSSTable(dir string, id string) (*SSTable, error) {
	f, err := os.Open(path.Join(dir, id+".sstable"))
	if err != nil {
		return nil, err
	}
	ss := new(SSTable)
	ss.f = f

	// generate sparseIndex
	indexList, err := genSparseIndex(f)
	if err != nil {
		return nil, err
	}
	ss.sparseIndex = indexList
	return ss, nil
}

func (ss *SSTable) Get(key []byte) ([]byte, error) {
	// fmt.Printf("getting key %s\n", hex.EncodeToString(key))
	startOffset := searchSparseIndex(ss.sparseIndex, key)
	if startOffset < 0 {
		fmt.Printf("key %s not in sparse index\n", hex.EncodeToString(key))
		return nil, fmt.Errorf("key not found")
	}
	offset := startOffset
	for {
		curKey, err := util.ReadVarLenBytes(ss.f, false, &offset)
		if err != nil {
			return nil, err
		}
		cmp := bytes.Compare(curKey, key)
		if cmp == 0 {
			val, err := util.ReadVarLenBytes(ss.f, false, &offset)
			if err != nil {
				return nil, err
			}
			return val, nil
		} else if cmp > 0 {
			// crossed expected position of the key
			fmt.Printf("crossed expected position\n")
			break
		} else {
			_, err := util.ReadVarLenBytes(ss.f, true, &offset)
			if err != nil {
				return nil, err
			}
		}
	}
	return nil, fmt.Errorf("key not found")
}
