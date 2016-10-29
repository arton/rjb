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
    add_jar(File.expand_path('./jartest.jar'))
    assert Rjb::import('jp.co.infoseek.hp.arton.rjb.JarTest2')
  end
end
