package types

import (
	"io"
)

type Value struct {
	Deleted bool
	Data    []byte
}

func (v *Value) WriteBytes(w io.Writer) (bytesWritten int, err error) {
	bytesWritten = 0
	if v.Deleted {
		n, err := w.Write([]byte{1})
		bytesWritten += n
		if err != nil {
			return bytesWritten, err
		}
	} else {
		n, err := w.Write([]byte{0})
		bytesWritten += n
		if err != nil {
			return bytesWritten, err
		}
		_, err = w.Write(v.Data)
		if err != nil {
			return bytesWritten, err
		}
	}
	return bytesWritten, nil
}

func (v *Value) ReadBytes(r io.Reader, size int) (err error) {
	// reads size bytes from r, and converts it into a Value
	data := make([]byte, size)
	_, err = io.ReadFull(r, data)
	if err != nil {
		return err
	}
	v.Deleted = (data[0] == 1)
	if !v.Deleted {
		v.Data = data[1:]
	}
	return nil
}
