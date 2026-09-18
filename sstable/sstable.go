package sstable

// SSTable
// Each SSTable consists of a single SSTable file and a corresponding sparse index
// <id>.keel.sstable file: contains the actual data
// <id>.keel.spindex file: contains the sparse index
// The file consists of a sequence of key value entries.
// Key: 4 byte length (big-endian), key in bytes
// Value: 4 byte length (big-endian), value in bytes

import (
	"bytes"
	"encoding/hex"
	"fmt"
	"io"
	"math"
	"os"
	"path"

	"github.com/aadit-n3rdy/keel/tree"
	"github.com/aadit-n3rdy/keel/types"
	"github.com/aadit-n3rdy/keel/util"
)

type SSTable struct {
	// manage handling an open SSTable file
	sparseIndex     []sstIndexEntry
	sstFile         *os.File
	sparseIndexFile *os.File
}

type sstIndexEntry struct {
	key    []byte
	offset int64
}

func sstableFilePath(dir string, id string) string {
	return path.Join(dir, id+".keel.sstable")
}

func sparseIndexFilePath(dir string, id string) string {
	return path.Join(dir, id+".keel.spindex")
}

func NewSSTableFile(dir string, id string, memtab *tree.Tree) error {
	// create a new SSTable with the given memtab
	sstableFile, err := os.Create(sstableFilePath(dir, id))
	if err != nil {
		return fmt.Errorf("failed to create SStable file %v: %w", id, err)
	}
	defer sstableFile.Close()

	sparseIndexFile, err := os.Create(sparseIndexFilePath(dir, id))
	if err != nil {
		return fmt.Errorf("failed to create sparse index file %v: %w", id, err)
	}
	defer sparseIndexFile.Close()

	err = writeSSTableFile(sstableFile, memtab)
	if err != nil {
		return fmt.Errorf("failed to write SSTable file %v: %w", id, err)
	}

	sparseIndex, err := genSparseIndex(sstableFile)
	if err != nil {
		return fmt.Errorf("failed to generate sparse index for table %v: %w", id, err)
	}

	err = writeSparseIndexFile(sparseIndexFile, sparseIndex)
	if err != nil {
		return fmt.Errorf("failed to write sparse index for sstable %v: %w", id, err)
	}

	return nil
}

func writeSSTableFile(f io.Writer, memtab *tree.Tree) error {
	err := memtab.ForEach(func(key []byte, value types.Value) error {
		err := util.WriteVarLenBytes(f, key)
		if err != nil {
			return fmt.Errorf("failed to write key: %w", err)
		}
		buf := bytes.NewBuffer(make([]byte, 0))
		_, err = value.WriteBytes(buf)
		if err != nil {
			return fmt.Errorf("failed to write value bytes to membuf: %w", err)
		}
		err = util.WriteVarLenBytes(f, buf.Bytes())
		if err != nil {
			return fmt.Errorf("failed to write value: %w", err)
		}
		return nil
	})
	return err
}

func OpenSSTable(dir string, id string) (*SSTable, error) {
	sstFile, err := os.Open(sstableFilePath(dir, id))
	if err != nil {
		return nil, fmt.Errorf("failed to open SSTable file, ID: %v, error: %w", id, err)
	}
	ss := new(SSTable)
	ss.sstFile = sstFile

	sparseIndexFile, err := os.Open(sparseIndexFilePath(dir, id))
	if err != nil {
		return nil, fmt.Errorf("failed to open Sparse Index file, ID: %v, error: %w", id, err)
	}
	ss.sparseIndexFile = sparseIndexFile

	indexList, err := readSparseIndexFile(sparseIndexFile)
	if err != nil {
		return nil, fmt.Errorf("failed to read Sparse Index file, ID: %v, error: %w", id, err)
	}
	ss.sparseIndex = indexList
	return ss, nil
}

func (ss *SSTable) Get(key []byte) (*types.Value, error) {
	// fmt.Printf("getting key %s\n", hex.EncodeToString(key))
	startIndex := searchSparseIndex(ss.sparseIndex, key)
	if startIndex < 0 {
		fmt.Printf("key %s not in sparse index\n", hex.EncodeToString(key))
		return nil, fmt.Errorf("key not found")
	}
	curKey := make([]byte, 0)
	val := make([]byte, 0)
	offset := 0
	n_bytes := 0
	offset = int(ss.sparseIndex[startIndex].offset)
	if startIndex != len(ss.sparseIndex)-1 {
		n_bytes = int(ss.sparseIndex[startIndex+1].offset) - offset
	} else {
		n_bytes = math.MaxInt64
	}
	queryReader := io.NewSectionReader(ss.sstFile, int64(offset), int64(n_bytes))
	for {
		curKey, err := util.ReadVarLenBytes(queryReader, curKey)
		if err != nil {
			return nil, fmt.Errorf("failed to read next key: %w", err)
		}
		cmp := bytes.Compare(curKey, key)
		if cmp == 0 {
			val, err = util.ReadVarLenBytes(queryReader, val)
			if err != nil {
				return nil, fmt.Errorf("failed to read value: %w", err)

			}
			result := types.Value{}
			err = result.ReadBytes(bytes.NewReader(val), len(val))
			if err != nil {
				return nil, fmt.Errorf("decoding value to object: %w", err)
			}
			return &result, nil
		} else if cmp > 0 {
			// fmt.Printf("crossed expected position\n")
			break
		} else {
			err = util.SkipVarLenBytes(queryReader)
			if err != nil {
				return nil, fmt.Errorf("failed to skip unneeded value: %w", err)
			}
		}
	}
	return nil, fmt.Errorf("key not found")
}
