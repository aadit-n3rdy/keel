package sstable

import (
	"bytes"
	"encoding/binary"
	"errors"
	"fmt"
	"io"
	"os"

	"github.com/aadit-n3rdy/keel/util"
)

// TODO: persist sparse index as part of file

func genSparseIndex(f *os.File) ([]sstIndexEntry, error) {
	index := make([]sstIndexEntry, 0)
	for {
		nextEntry := sstIndexEntry{key: nil, offset: 0}
		err := genSingleSparseIndexEntry(f, &nextEntry)
		if errors.Is(err, io.EOF) {
			index = append(index, nextEntry)
			// fmt.Printf("Finished generating sparse index\n")
			break
		}
		if err != nil {
			return nil, fmt.Errorf("failed to generate sparse index entry: %w", err)
		}
		index = append(index, nextEntry)
	}
	return index, nil
}

func searchSparseIndex(index []sstIndexEntry, key []byte) int {
	// returns the index of the sparse index at/after which the key can be found
	lo := 0
	hi := len(index) - 1
	for lo < hi {
		mid := (lo + hi) / 2
		if bytes.Compare(key, index[mid].key) < 0 {
			hi = mid - 1
		} else {
			if lo == mid {
				// edge case: 2 elements
				if bytes.Compare(key, index[mid+1].key) < 0 {
					hi = lo
				} else {
					lo = lo + 1
				}
			} else {
				lo = mid
			}
		}
	}
	if bytes.Compare(index[lo].key, key) <= 0 {
		return lo
	} else {
		return -1
	}
}

func genSingleSparseIndexEntry(f *os.File, result *sstIndexEntry) error {
	// read 1 KV pair, then skip SPARSE_INDEX_CHUNK_SIZE-1 KV pairs
	offset, _ := f.Seek(0, io.SeekCurrent)
	nextKey, err := util.ReadVarLenBytes(f, nil)
	if err != nil {
		return fmt.Errorf("failed to read key length: %w", err)
	}
	skipLen, err := util.GetVarLenBytesSize(f)
	_, err = f.Seek(int64(skipLen), 1)
	result.key = nextKey
	result.offset = offset
	// fmt.Printf("sparse index read key %s at offset %d\n", hex.EncodeToString(nextKey), offset)
	if err != nil {
		return fmt.Errorf("failed to get value length: %w", err)
	}
	for range SPARSE_INDEX_CHUNK_SIZE - 1 {
		err = util.SkipVarLenBytes(f)
		if err != nil {
			// fmt.Printf("skip while reading next key\n")
			return fmt.Errorf("failed to get skip key: %w", err)
		}
		err = util.SkipVarLenBytes(f)
		if err != nil {
			// fmt.Printf("skip while reading next value\n")
			return fmt.Errorf("failed to get skip value: %w", err)
		}
		// fmt.Printf("sparse index skipped a key\n")
	}
	return nil
}

func (entry *sstIndexEntry) writeTo(f io.Writer) error {
	err := util.WriteVarLenBytes(f, entry.key)
	if err != nil {
		return err
	}
	offsetBytes, err := binary.Append(nil, binary.BigEndian, int64(entry.offset))
	if err != nil {
		return err
	}

	_, err = f.Write(offsetBytes)
	if err != nil {
		return err
	}

	return nil
}

func ReadIndexEntryFrom(f io.Reader) (sstIndexEntry, error) {
	entry := sstIndexEntry{}
	var err error
	entry.key, err = util.ReadVarLenBytes(f, nil)
	if err != nil {
		return entry, err
	}
	offsetBuf := make([]byte, 8)
	_, err = io.ReadFull(f, offsetBuf)
	if err != nil {
		return entry, err
	}
	_, err = binary.Decode(offsetBuf, binary.BigEndian, &entry.offset)
	return entry, err
}

func writeSparseIndexFile(f io.Writer, sparseIndex []sstIndexEntry) error {
	var err error
	if len(sparseIndex) == 0 {
		return nil
	}
	for _, sparseIndexEntry := range sparseIndex {
		err = sparseIndexEntry.writeTo(f)
		if err != nil {
			return err
		}
	}
	return nil
}

func readSparseIndexFile(f io.Reader) ([]sstIndexEntry, error) {
	var err error = nil
	indexList := make([]sstIndexEntry, 0)
	for err == nil {
		var indexEntry sstIndexEntry
		indexEntry, err = ReadIndexEntryFrom(f)
		if err == nil {
			indexList = append(indexList, indexEntry)
		}
	}
	if !errors.Is(err, io.EOF) {
		return nil, fmt.Errorf("failed to read sparse indices, error: %w", err)
	}
	return indexList, nil
}
