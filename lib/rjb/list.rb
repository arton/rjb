# encoding: utf-8
=begin
  Copyright(c) 2012 arton
=end

require 'rjb'

module Rjb
  JIterable = import('java.lang.Iterable')
  JIterator = import('java.util.Iterator')
  module Iterable
    def each
      it = iterator
      while it.has_next
        yield it.next
      end
    end
  end
  module Iterator
    def each
      while has_next
        yield self.next
      end
    end
  end
  class Rjb_JavaProxy
    def initialize_proxy
      if JIterable.isInstance(self)
        include Iterable
        include Enumerable
      elsif JIterator.isInstance(self)
        include Iterator
        include Enumerable
      end
    end
  end
end
