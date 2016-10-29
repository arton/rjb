#!/usr/local/env ruby -Ku
# encoding: utf-8
# $Id: test.rb 176 2011-11-09 14:27:28Z arton $

begin
  require 'rjb'
rescue LoadError 
  require 'rubygems' 
  require 'rjb'
end
require 'test/unit'

class TestUnloadRjb < Test::Unit::TestCase
  include Rjb
  def setup
    Rjb::load('.')
  end
  
  def test_unload
    jString = import('java.lang.String')
    assert_equal 0, Rjb::unload
    jString = nil
    GC.start
  end
end

