// automatically generated by the FlatBuffers compiler, do not modify


#ifndef FLATBUFFERS_GENERATED_BASIC_TABLES_H_
#define FLATBUFFERS_GENERATED_BASIC_TABLES_H_

#include "flatbuffers/flatbuffers.h"

namespace Tables {

struct Basic;

struct Basic FLATBUFFERS_FINAL_CLASS : private flatbuffers::Table {
  enum FlatBuffersVTableOffset FLATBUFFERS_VTABLE_UNDERLYING_TYPE {
    VT_DATA = 4
  };
  const flatbuffers::Vector<uint64_t> *data() const {
    return GetPointer<const flatbuffers::Vector<uint64_t> *>(VT_DATA);
  }
  bool Verify(flatbuffers::Verifier &verifier) const {
    return VerifyTableStart(verifier) &&
           VerifyOffset(verifier, VT_DATA) &&
           verifier.VerifyVector(data()) &&
           verifier.EndTable();
  }
};

struct BasicBuilder {
  flatbuffers::FlatBufferBuilder &fbb_;
  flatbuffers::uoffset_t start_;
  void add_data(flatbuffers::Offset<flatbuffers::Vector<uint64_t>> data) {
    fbb_.AddOffset(Basic::VT_DATA, data);
  }
  explicit BasicBuilder(flatbuffers::FlatBufferBuilder &_fbb)
        : fbb_(_fbb) {
    start_ = fbb_.StartTable();
  }
  BasicBuilder &operator=(const BasicBuilder &);
  flatbuffers::Offset<Basic> Finish() {
    const auto end = fbb_.EndTable(start_);
    auto o = flatbuffers::Offset<Basic>(end);
    return o;
  }
};

inline flatbuffers::Offset<Basic> CreateBasic(
    flatbuffers::FlatBufferBuilder &_fbb,
    flatbuffers::Offset<flatbuffers::Vector<uint64_t>> data = 0) {
  BasicBuilder builder_(_fbb);
  builder_.add_data(data);
  return builder_.Finish();
}

inline flatbuffers::Offset<Basic> CreateBasicDirect(
    flatbuffers::FlatBufferBuilder &_fbb,
    const std::vector<uint64_t> *data = nullptr) {
  auto data__ = data ? _fbb.CreateVector<uint64_t>(*data) : 0;
  return Tables::CreateBasic(
      _fbb,
      data__);
}

inline const Tables::Basic *GetBasic(const void *buf) {
  return flatbuffers::GetRoot<Tables::Basic>(buf);
}

inline const Tables::Basic *GetSizePrefixedBasic(const void *buf) {
  return flatbuffers::GetSizePrefixedRoot<Tables::Basic>(buf);
}

inline bool VerifyBasicBuffer(
    flatbuffers::Verifier &verifier) {
  return verifier.VerifyBuffer<Tables::Basic>(nullptr);
}

inline bool VerifySizePrefixedBasicBuffer(
    flatbuffers::Verifier &verifier) {
  return verifier.VerifySizePrefixedBuffer<Tables::Basic>(nullptr);
}

inline void FinishBasicBuffer(
    flatbuffers::FlatBufferBuilder &fbb,
    flatbuffers::Offset<Tables::Basic> root) {
  fbb.Finish(root);
}

inline void FinishSizePrefixedBasicBuffer(
    flatbuffers::FlatBufferBuilder &fbb,
    flatbuffers::Offset<Tables::Basic> root) {
  fbb.FinishSizePrefixed(root);
}

}  // namespace Tables

#endif  // FLATBUFFERS_GENERATED_BASIC_TABLES_H_