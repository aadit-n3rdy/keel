package sstable

import (
	"bytes"
	"errors"
	"io"
	"os"

	"github.com/aadit-n3rdy/keel/util"
)

func genSparseIndex(f *os.File) ([]sstIndexEntry, error) {
	index := make([]sstIndexEntry, 0)
	for {
		nextEntry := sstIndexEntry{key: nil, offset: 0}
		err := readSparseIndexEntry(f, &nextEntry)
		if errors.Is(err, io.EOF) {
			index = append(index, nextEntry)
			// fmt.Printf("Finished generating sparse index\n")
			break
		}
		if err != nil {
			return nil, err
		}
		index = append(index, nextEntry)
	}
	return index, nil
}

func searchSparseIndex(index []sstIndexEntry, key []byte) int64 {
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
		return index[lo].offset
	} else {
		return -1
	}
}

func readSparseIndexEntry(f *os.File, result *sstIndexEntry) error {
	// read 1 KV pair, then skip SPARSE_INDEX_CHUNK_SIZE-1 KV pairs
	offset, _ := f.Seek(0, io.SeekCurrent)
	nextKey, err := util.ReadVarLenBytes(f, false, nil)
	if err != nil && err != io.EOF {
		return err
	}
	_, err = util.ReadVarLenBytes(f, true, nil)
	result.key = nextKey
	result.offset = offset
	// fmt.Printf("sparse index read key %s at offset %d\n", hex.EncodeToString(nextKey), offset)
	if err != nil {
		return err
	}
	for _ = range SPARSE_INDEX_CHUNK_SIZE - 1 {
		_, err = util.ReadVarLenBytes(f, true, nil)
		if err != nil {
			// fmt.Printf("skip while reading next key\n")
			return err
		}
		_, err = util.ReadVarLenBytes(f, true, nil)
		if err != nil {
			// fmt.Printf("skip while reading next value\n")
			return err
		}
		// fmt.Printf("sparse index skipped a key\n")
	}
	return nil
}
