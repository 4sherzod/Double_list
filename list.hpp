#include <algorithm>
#include <stdexcept>

template <typename T, typename Alloc = std::allocator<T>>
class List {
 private:
  struct Node;

 public:
  template <bool IsConst>
  class CommonIterator {
   public:
    using difference_type = std::ptrdiff_t;
    using value_type = std::conditional_t<IsConst, const T, T>;
    using pointer = value_type*;
    using reference = value_type&;
    using iterator_category = std::bidirectional_iterator_tag;

   private:
    using node_t = std::conditional_t<IsConst, const Node*, Node*>;

    node_t ptr_ = nullptr;

   public:
    explicit CommonIterator(node_t ptr) : ptr_(ptr) {}
    CommonIterator() = delete;

    explicit operator CommonIterator<true>() {
      return CommonIterator<true>(ptr_);
    }

    value_type operator*() const { return ptr_->value; }

    reference operator*() { return ptr_->value; }

    pointer operator->() { return &(ptr_->value); }

    constexpr CommonIterator<IsConst>& operator++() {
      ptr_ = ptr_->next;
      return *this;
    }

    constexpr CommonIterator<IsConst>& operator--() {
      ptr_ = ptr_->prev;
      return *this;
    }

    constexpr CommonIterator<IsConst> operator++(int) {
      CommonIterator<IsConst> temp = *this;
      ++(*this);
      return temp;
    }

    constexpr CommonIterator<IsConst> operator--(int) {
      CommonIterator<IsConst> temp = *this;
      --(*this);
      return temp;
    }

    constexpr bool operator==(const CommonIterator<IsConst>& other) const {
      return ptr_ == other.ptr_;
    }

    constexpr bool operator!=(const CommonIterator<IsConst>& other) const {
      return !(*this == other);
    }

    node_t get_ptr() { return ptr_; }
  };

  using value_type = T;
  using allocator_type = Alloc;
  using iterator = CommonIterator<false>;

  using size_type = size_t;
  using reference = value_type&;
  using const_reference = const value_type&;
  using rvalue_reference = value_type&&;

  using const_iterator = CommonIterator<true>;
  using reverse_iterator = std::reverse_iterator<iterator>;
  using const_reverse_iterator = std::reverse_iterator<const_iterator>;

  List() { fake_allocate(); }
  explicit List(const allocator_type& alloc) : alloc_(alloc) {
    fake_allocate();
  }
  explicit List(size_type count, const T& value,
                const allocator_type& alloc = allocator_type())
      : alloc_(alloc) {
    fake_allocate();
    try {
      for (size_t i = 0; i < count; ++i) {
        push_back(value);
      }
    } catch (...) {
      clear();
      rebind_alloc_traits::deallocate(alloc_, fake_, 1);
      throw;
    }
  }

  explicit List(size_type count, const allocator_type& alloc = allocator_type())
      : alloc_(alloc) {
    fake_allocate();
    try {
      for (size_t i = 0; i < count; ++i) {
        push_back();
      }
    } catch (...) {
      clear();
      rebind_alloc_traits::deallocate(alloc_, fake_, 1);
      throw;
    }
  }
  List(const List& other)
      : alloc_(rebind_alloc_traits::select_on_container_copy_construction(
            other.alloc_)) {
    fake_allocate();
    auto iter = other.begin();
    try {
      for (size_t i = 0; i < other.size(); ++i) {
        push_back(*iter);
        ++iter;
      }
    } catch (...) {
      clear();
      rebind_alloc_traits::deallocate(alloc_, fake_, 1);
      throw;
    }
  }

  List(std::initializer_list<value_type> init,
       const allocator_type& alloc = allocator_type())
      : alloc_(alloc) {
    fake_allocate();
    auto iter = init.begin();
    try {
      for (size_t i = 0; i < init.size(); ++i) {
        push_back(*iter);
        ++iter;
      }
    } catch (...) {
      clear();
      rebind_alloc_traits::deallocate(alloc_, fake_, 1);
      throw;
    }
  }

  ~List() {
    clear();
    rebind_alloc_traits::deallocate(alloc_, fake_, 1);
  }

  size_type size() const { return size_; }
  bool empty() const { return size_ == 0; }
  allocator_type get_allocator() const noexcept { return alloc_; }

  iterator begin() const {
    return iterator(fake_->next == nullptr ? fake_ : fake_->next);
  }
  iterator end() const { return iterator(fake_); }

  const_iterator cbegin() const {
    return const_iterator(fake_->next == nullptr ? fake_ : fake_->next);
  }
  const_iterator cend() const { return const_iterator(fake_); }

  reference front() { return fake_->next->value; }
  const_reference front() const { return fake_->next->value; }

  reference back() { return fake_->prev->value; }
  const_reference back() const { return fake_->prev->value; }

  void clear() {
    if (empty()) {
      return;
    }
    Node* cur = fake_->next;
    while (cur != fake_) {
      Node* tmp = cur;
      cur = cur->next;
      destroy_node(alloc_, tmp);
    }
    size_ = 0;
  }

  void pop_back() {
    if (empty()) {
      throw std::out_of_range("Pop empty list\n");
    }
    del(fake_->prev);
  }
  void pop_front() {
    if (empty()) {
      throw std::out_of_range("Pop empty list\n");
    }
    del(fake_->next);
  }
  void push_back(const T& value) {
    Node* tmp = rebind_alloc_traits::allocate(alloc_, 1);
    try {
      rebind_alloc_traits::construct(alloc_, tmp, fake_->prev, fake_,
                                     std::move(value));
    } catch (...) {
      rebind_alloc_traits::deallocate(alloc_, tmp, 1);
      throw;
    }
    fake_->prev->next = tmp;
    fake_->prev = tmp;
    ++size_;
  }
  void push_back() {
    Node* tmp = rebind_alloc_traits::allocate(alloc_, 1);
    try {
      rebind_alloc_traits::construct(alloc_, tmp, fake_->prev, fake_);
    } catch (...) {
      rebind_alloc_traits::deallocate(alloc_, tmp, 1);
      throw;
    }
    fake_->prev->next = tmp;
    fake_->prev = tmp;
    ++size_;
  }
  void push_front(const T& value) {
    Node* tmp = rebind_alloc_traits::allocate(alloc_, 1);
    try {
      rebind_alloc_traits::construct(alloc_, tmp, fake_, fake_->next, value);
    } catch (...) {
      rebind_alloc_traits::deallocate(alloc_, tmp, 1);
      throw;
    }
    fake_->next->prev = tmp;
    fake_->next = tmp;
    ++size_;
  }
  void push_back(T&& value) {
    Node* tmp = rebind_alloc_traits::allocate(alloc_, 1);
    try {
      rebind_alloc_traits::construct(alloc_, tmp, fake_->prev, fake_,
                                     std::move(value));
    } catch (...) {
      rebind_alloc_traits::deallocate(alloc_, tmp, 1);
      throw;
    }
    fake_->prev->next = tmp;
    fake_->prev = tmp;
    ++size_;
  }
  void push_front(T&& value) {
    Node* tmp = rebind_alloc_traits::allocate(alloc_, 1);
    try {
      rebind_alloc_traits::construct(alloc_, tmp, fake_, fake_->next, value);
    } catch (...) {
      rebind_alloc_traits::deallocate(alloc_, tmp, 1);
      throw;
    }
    fake_->next->prev = tmp;
    fake_->next = tmp;
    ++size_;
  }
  List& operator=(const List& other) {
    List temp(other);
    clear();
    swap(temp);
    if constexpr (rebind_alloc_traits::propagate_on_container_copy_assignment::
                      value) {
      if (alloc_ != other.alloc_) {
        alloc_ = other.alloc_;
      }
    }
    return *this;
  }

  reverse_iterator rbegin() const { return reverse_iterator(fake_->prev); }

 private:
  struct Node {
    explicit Node(Node* prev = nullptr, Node* next = nullptr)
        : next(next), prev(prev) {}

    explicit Node(Node* prev, Node* next, const T& value)
        : next(next), prev(prev), value(value) {}
    explicit Node(Node* prev, Node* next, T&& value)
        : next(next), prev(prev), value(std::move(value)) {}

    explicit Node(const T& value) : value(value) {}
    explicit Node(T&& value) : value(std::move(value)) {}
    Node* next = nullptr;
    Node* prev = nullptr;
    value_type value;
  };

  using alloc_traits = std::allocator_traits<allocator_type>;
  using rebind_allocator_type =
      typename alloc_traits::template rebind_alloc<Node>;
  using rebind_alloc_traits =
      typename alloc_traits::template rebind_traits<Node>;

  size_t size_ = 0;
  rebind_allocator_type alloc_;
  Node* fake_ = nullptr;

  void destroy_node(rebind_allocator_type& alloc, Node* node) {
    rebind_alloc_traits::destroy(alloc, node);
    rebind_alloc_traits::deallocate(alloc, node, 1);
  }

  void swap(List<T, Alloc>& other) {
    std::swap(size_, other.size_);
    std::swap(fake_, other.fake_);
    std::swap(alloc_, other.alloc_);
  }
  void del(Node* ptr) {
    if (ptr == fake_) {
      return;
    }
    ptr->prev->next = ptr->next;
    ptr->next->prev = ptr->prev;
    rebind_alloc_traits::destroy(alloc_, ptr);
    rebind_alloc_traits::deallocate(alloc_, ptr, 1);
    --size_;
  }
  void fake_allocate() {
    fake_ = rebind_alloc_traits::allocate(alloc_, 1);
    fake_->prev = fake_->next = fake_;
  }
};