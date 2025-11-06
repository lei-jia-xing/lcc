#pragma once
#include <memory>
#include <string>
#include <vector>

class Type;

using TypePtr = std::shared_ptr<Type>;

enum class BaseType { VOID, INT };

class Type {
public:
  enum class Category { Basic, Array, Function };

  Category category;
  BaseType base_type;
  bool is_const = false;
  bool is_static = false;

  TypePtr array_element_type;
  int array_size = 0;

  TypePtr return_type;
  std::vector<TypePtr> params;

  Type(Category cat) : category(cat) {}

  static TypePtr create_base_type(BaseType base, bool is_const = false,
                                  bool is_static = false) {
    auto type = std::make_shared<Type>(Category::Basic);
    type->is_const = is_const;
    type->is_static = is_static;
    type->base_type = base;
    return type;
  }

  static TypePtr create_array_type(TypePtr element_type, int size) {
    auto type = std::make_shared<Type>(Category::Array);
    type->array_element_type = element_type;
    type->array_size = size;
    return type;
  }

  static TypePtr create_function_type(TypePtr ret_type,
                                      const std::vector<TypePtr> &params) {
    auto type = std::make_shared<Type>(Category::Function);
    type->return_type = ret_type;
    type->params = params;
    return type;
  }
  static TypePtr getIntType() { return create_base_type(BaseType::INT); }
  static TypePtr getVoidType() { return create_base_type(BaseType::VOID); }
};

inline std::string to_string(const TypePtr &type) {
  if (!type) {
    return "Unknown";
  }

  if (type->category == Type::Category::Basic) {
    if (type->base_type == BaseType::INT) {
      if (type->is_const) {
        return "ConstInt";
      } else if (type->is_static) {
        return "StaticInt";
      } else {
        return "Int";
      }
    } else if (type->base_type == BaseType::VOID) {
      return "Void";
    }
  } else if (type->category == Type::Category::Array) {
    if (type->array_element_type &&
        type->array_element_type->base_type == BaseType::INT) {
      if (type->array_element_type->is_const) {
        return "ConstIntArray";
      } else if (type->array_element_type->is_static) {
        return "StaticIntArray";
      } else {
        return "IntArray";
      }
    }
  } else if (type->category == Type::Category::Function) {
    if (type->return_type && type->return_type->base_type == BaseType::VOID) {
      return "VoidFunc";
    } else if (type->return_type &&
               type->return_type->base_type == BaseType::INT) {
      return "IntFunc";
    }
  }
  return "Unknown";
}
