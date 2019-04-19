#include <bits/stl_pair.h>

template <typename _Tp>
class SingleList {
public:
  typedef _Tp value_type;

private:
  struct Node {
    value_type val;
    Node* next;

    Node() : next(nullptr) {}
    explicit Node(value_type& val) : val(val), next(nullptr) {}
  };

public:
  struct iterator {
    Node* node;

    explicit iterator(Node* node) : node(node) {}

    value_type& operator*() {
      return node->val;
    }

    value_type* operator->() {
      return &(node->val);
    }

    SingleList<value_type>::iterator& operator++() {
      node = node->next;
      return *this;
    }

    const SingleList<value_type>::iterator operator++(int) {
      auto tmp = this;
      node = node->next;
      return *tmp;
    }

    bool operator==(const SingleList<value_type>::iterator& val) {
      return node == val.node;
    }

    bool operator!=(const SingleList<value_type>::iterator& val) {
      return node != val.node;
    }

    SingleList<value_type>::iterator operator+(int n) {
      Node* new_node = node;
      for (; n; n--) {
        new_node = new_node->next;
      }
      return SingleList<value_type>::iterator(new_node);
    }
  };


private:
  std::size_t len;

  Node* head;
  Node* tail;

public:
  SingleList() : len(0) {
    Node* dummy = new Node();
    head = dummy;
    tail = dummy;
  }

  ~SingleList() {
    Node* p = head;
    while (p) {
      Node* tmp = p->next;
      delete(p);
      p = tmp;
    }
  }

  SingleList<value_type>::iterator begin() {
    return SingleList<value_type>::iterator(head->next);
  }

  SingleList<value_type>::iterator end() {
    return SingleList<value_type>::iterator(tail->next);
  }

  SingleList<value_type>::iterator insert_behind(SingleList<value_type>::iterator& pos, value_type&& val) {
    Node* new_node = Node(val);

    new_node->next = pos.node->next;
    pos.node->next = new_node;
    len++;

    return SingleList<value_type>::iterator(new_node);
  }

  void erase_behind(SingleList<value_type>::iterator& pos, int n) {
    Node* p = pos.node->next;
    for (; n; n--) {
      Node* tmp = p->next;
      delete(p);
      p = tmp;
    }
    len -= n;
  }

  void push_front(value_type&& val) {
    Node* new_node = new Node(val);

    new_node->next = head->next;
    head->next = new_node;
    len++;
  }

  void pop_front() {
    Node* tmp = head->next;
    delete(head);
    head = tmp;
    len--;
  }

  void push_back(value_type&& val) {
    Node* new_node = new Node(val);

    tail->next = new_node;
    tail = tail->next;
    len++;
  }

  void pop_back() {
    delete(tail);
    for (auto p = head; p->next; p = p->next)
      tail = p;
    len--;
  }

  void move_next_to_tail(SingleList<value_type>::iterator&& pos) {
    Node* pos_next = pos.node->next;
    pos.node->next = pos_next->next;
    pos_next->next = nullptr;

    tail->next = pos_next;
    tail = tail->next;
  }

  void move_head_to_tail() {
    if (head == tail || head->next == tail)
      return;

    Node* pos_next = head->next;
    head->next = pos_next->next;
    pos_next->next = nullptr;

    tail->next = pos_next;
    tail = tail->next;
  }

  std::size_t size() {
    return len;
  }

};

template <typename _KEY, typename _Tp>
class Cache {
public:
  typedef _KEY key_type;
  typedef _Tp  val_type;

private:
  std::size_t max_len;

  SingleList<std::pair<key_type, val_type>> list;

public:
  explicit Cache(std::size_t max_len) : max_len(max_len) {}

  // put new element to tail of list
  void push(key_type key, val_type val) {
    list.push_back(make_pair(key, val));
    if (list.size() > max_len) {
      // oldest element is in front of the list
      list.pop_front();
    }
  }

  bool get(key_type key, val_type& val) {
    // counter is the distance of target and begin(). so I can move it to tail of list.
    int counter = -1;
    auto iter = list.begin();

    // find element
    for (; iter != list.end(); iter++) {
      if (iter->first == key) break;
      counter++;
    }

    if (iter == list.end()) return false;
    val = iter->second;

    if (counter == -1) {
      list.move_head_to_tail();
    } else {
      list.move_next_to_tail(list.begin() + counter);
    }

    return true;
  }
};

