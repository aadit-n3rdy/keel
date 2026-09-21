package sstable

import (
	"bytes"
	"os"
	"testing"
)

func TestSparseIndexReadWrite(t *testing.T) {
	byteBuf := bytes.NewBuffer(nil)
	actualEntry := sstIndexEntry{
		offset: 1234,
		key:    []byte{0xab, 0x12, 0x00, 0x34},
	}
	err := actualEntry.writeTo(byteBuf)
	if err != nil {
		t.Fatalf("failed writing to bytebuf %v", err.Error())
	}
	readEntry, err := ReadIndexEntryFrom(byteBuf)
	if err != nil {
		t.Fatalf("failed reading from bytebuf %v", err.Error())
	}
	if readEntry.offset != actualEntry.offset {
		t.Errorf("read entry offset %v did not match actual entry offset %v", readEntry.offset, actualEntry.offset)
	}
	if !bytes.Equal(readEntry.key, actualEntry.key) {
		t.Errorf("read entry bytes %v did not match actual entry bytes %v", readEntry.key, actualEntry.key)
	}
}

func TestSparseIndexFileReadWrite(t *testing.T) {
	f, err := os.Create("./test.spindex")
	if err != nil {
		t.Fatalf("%v", err)
	}
	defer os.Remove("./test.spindex")

	sparseIndex := []sstIndexEntry{
		{
			offset: 1234,
			key:    []byte{0xab, 0x12, 0x00, 0x34},
		},
	}
	err = writeSparseIndexFile(f, sparseIndex)
	if err != nil {
		t.Fatalf("%v", err)
	}

	f.Close()
	f, err = os.Open("./test.spindex")
	if err != nil {
		t.Fatalf("%v", err)
	}

	readIndex, err := readSparseIndexFile(f)
	if err != nil {
		t.Fatalf("%v", err)
	}
	if len(readIndex) != len(sparseIndex) {
		t.Errorf("readIndex and original sparseIndex not of same length")
	}
}
