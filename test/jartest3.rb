#!/usr/local/env ruby -Ku
# encoding: utf-8

begin
  require 'rjb'
rescue LoadError 
  require 'rubygems' 
  require 'rjb'
end
require 'test/unit'

class JarTest < Test::Unit::TestCase
  include Rjb
  
  def setup
    Rjb::load()
  end
  
  def test_depends
    add_jar(File.expand_path('./jartest2.jar'))    
    begin
      Rjb::import('jp.co.infoseek.hp.arton.rjb.JarTest2')
      fail 'no exception'
    rescue NoClassDefFoundError
      assert true
    end
  end
end
