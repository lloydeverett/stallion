#pragma once

#include <memory>

#include <boost/cobalt.hpp>
#include <boost/json.hpp>

enum class IotaKind {
  UNSPECIFIED,
  REPRESENTATIONAL,
  PRESENTATIONAL,
};

class MaybeIotaBase {
public:
  virtual IotaKind kind() { return IotaKind::UNSPECIFIED; };
  virtual std::string id() = 0;
  virtual ~MaybeIotaBase() = default;
};

class Iota : public MaybeIotaBase {
public:
  virtual std::string title() = 0;
  virtual std::string description() = 0;
  virtual boost::json::object json() = 0;
};

template <typename MaterializeT, typename DerivedT> class Materializable {
public:
  virtual std::pair<boost::cobalt::promise<std::unique_ptr<MaterializeT>>, int>
  materialize(std::unique_ptr<DerivedT> this_unique_ptr) = 0;
  virtual ~Materializable() = default;
};

class MaybeIota : public MaybeIotaBase,
                  public Materializable<Iota, MaybeIota> {};

class MaybeDirectory;

class IMaybeDirectory {
public:
  virtual std::unique_ptr<MaybeDirectory>
  get_directory(std::string_view id) = 0;
  virtual std::unique_ptr<MaybeIota> get_leaf(std::string_view id) = 0;
  virtual ~IMaybeDirectory() = default;
};

class IDirectory : public IMaybeDirectory {
public:
  virtual boost::cobalt::generator<std::unique_ptr<Iota>> items() = 0;
  virtual boost::cobalt::promise<boost::json::object> json_tree() = 0;
};

class Directory : public IDirectory, public Iota {};

class MaybeDirectory : public IMaybeDirectory,
                       public MaybeIotaBase,
                       public Materializable<Directory, MaybeDirectory> {};
