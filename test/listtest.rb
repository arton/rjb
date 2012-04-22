#!/usr/local/env ruby -Ku
# encoding: utf-8
=begin
  Copyright(c) 2012 arton
=end

begin
  require 'rjb/list'
rescue LoadError 
  require 'rubygems' 
  require 'rjb/list'
end
require 'test/unit'
require 'fileutils'

class ListTest < Test::Unit::TestCase
  include Rjb
  def test_create
    ja = import('java.util.ArrayList')
    a = ja.new
    a.add(1)
    a.add(2)
    a.add(3)
    n = 1
    a.each do |x|
      assert_equal n, x.intValue
      n += 1
    end
    assert_equal 4, n
  end
  def test_returned_proxy
    ja = import('java.util.Arrays')
    a = ja.as_list([1, 2, 3])
    n = 1
    a.each do |x|
      assert_equal n, x.intValue
      n += 1
    end
    assert_equal 4, n
  end
  def test_iterator
    ja = import('java.util.Arrays')
    it = ja.as_list([1, 2, 3]).iterator
    n = 1
    it.each do |x|
      assert_equal n, x.intValue
      n += 1
    end
    assert_equal 4, n
  end
  def test_enumerable
    ja = import('java.util.Arrays')
    assert_equal 55, ja.as_list((1..10).to_a).inject(0) {|r, e| r + e.intValue}
  end
end

