#!/usr/local/env ruby -Ku
# encoding: utf-8
# $Id:$

begin
  require 'rjb'
rescue LoadError 
  require 'rubygems' 
  require 'rjb'
end
require 'test/unit'

if RUBY_PLATFORM =~ /darwin/
  class TestOsxJvm < Test::Unit::TestCase
    def test_with_javahome
      ENV['JAVA_HOME'] = `/usr/libexec/java_home`
      assert_nothing_raised do
        Rjb::load
      end
    end
  end
end

