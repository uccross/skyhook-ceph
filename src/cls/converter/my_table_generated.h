// automatically generated by the FlatBuffers compiler, do not modify


#ifndef FLATBUFFERS_GENERATED_MYTABLE_CONVERSION_H_
#define FLATBUFFERS_GENERATED_MYTABLE_CONVERSION_H_

#include "flatbuffers/flatbuffers.h"

namespace conversion {

struct MyTable;

struct MyTable FLATBUFFERS_FINAL_CLASS : private flatbuffers::Table {
  enum FlatBuffersVTableOffset FLATBUFFERS_VTABLE_UNDERLYING_TYPE {
    VT_ID = 4,
    VT_DATA = 6
  };
  uint64_t id() const {
    return GetField<uint64_t>(VT_ID, 0);
  }
  const flatbuffers::Vector<uint64_t> *data() const {
    return GetPointer<const flatbuffers::Vector<uint64_t> *>(VT_DATA);
  }
  bool Verify(flatbuffers::Verifier &verifier) const {
    return VerifyTableStart(verifier) &&
           VerifyField<uint64_t>(verifier, VT_ID) &&
           VerifyOffset(verifier, VT_DATA) &&
           verifier.VerifyVector(data()) &&
           verifier.EndTable();
  }
};

struct MyTableBuilder {
  flatbuffers::FlatBufferBuilder &fbb_;
  flatbuffers::uoffset_t start_;
  void add_id(uint64_t id) {
    fbb_.AddElement<uint64_t>(MyTable::VT_ID, id, 0);
  }
  void add_data(flatbuffers::Offset<flatbuffers::Vector<uint64_t>> data) {
    fbb_.AddOffset(MyTable::VT_DATA, data);
  }
  explicit MyTableBuilder(flatbuffers::FlatBufferBuilder &_fbb)
        : fbb_(_fbb) {
    start_ = fbb_.StartTable();
  }
  MyTableBuilder &operator=(const MyTableBuilder &);
  flatbuffers::Offset<MyTable> Finish() {
    const auto end = fbb_.EndTable(start_);
    auto o = flatbuffers::Offset<MyTable>(end);
    return o;
  }
};

inline flatbuffers::Offset<MyTable> CreateMyTable(
    flatbuffers::FlatBufferBuilder &_fbb,
    uint64_t id = 0,
    flatbuffers::Offset<flatbuffers::Vector<uint64_t>> data = 0) {
  MyTableBuilder builder_(_fbb);
  builder_.add_id(id);
  builder_.add_data(data);
  return builder_.Finish();
}

inline flatbuffers::Offset<MyTable> CreateMyTableDirect(
    flatbuffers::FlatBufferBuilder &_fbb,
    uint64_t id = 0,
    const std::vector<uint64_t> *data = nullptr) {
  auto data__ = data ? _fbb.CreateVector<uint64_t>(*data) : 0;
  return conversion::CreateMyTable(
      _fbb,
      id,
      data__);
}

inline const conversion::MyTable *GetMyTable(const void *buf) {
  return flatbuffers::GetRoot<conversion::MyTable>(buf);
}

inline const conversion::MyTable *GetSizePrefixedMyTable(const void *buf) {
  return flatbuffers::GetSizePrefixedRoot<conversion::MyTable>(buf);
}

inline bool VerifyMyTableBuffer(
    flatbuffers::Verifier &verifier) {
  return verifier.VerifyBuffer<conversion::MyTable>(nullptr);
}

inline bool VerifySizePrefixedMyTableBuffer(
    flatbuffers::Verifier &verifier) {
  return verifier.VerifySizePrefixedBuffer<conversion::MyTable>(nullptr);
}

inline void FinishMyTableBuffer(
    flatbuffers::FlatBufferBuilder &fbb,
    flatbuffers::Offset<conversion::MyTable> root) {
  fbb.Finish(root);
}

inline void FinishSizePrefixedMyTableBuffer(
    flatbuffers::FlatBufferBuilder &fbb,
    flatbuffers::Offset<conversion::MyTable> root) {
  fbb.FinishSizePrefixed(root);
}

}  // namespace conversion

#endif  // FLATBUFFERS_GENERATED_MYTABLE_CONVERSION_H_
