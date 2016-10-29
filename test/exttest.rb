#!/usr/local/env ruby -Ku
# encoding: utf-8
# $Id:$

begin
  require 'rjb'
rescue LoadError 
  require 'rubygems' 
  require 'rjb'
end

if Rjb::VERSION < '1.2.2'
  puts "Rjb #{Rjb::VERSION} does not support rjbextension. exit"
  exit 0
end

require 'rjbextension'
require 'test/unit'
require 'fileutils'

FileUtils.rm_f 'jp/co/infoseek/hp/arton/rjb/Base.class'

puts "start RJB(#{Rjb::VERSION}) test"
class ExtTestRjb < Test::Unit::TestCase

  def jp
    JavaPackage.new('jp')
  end
  
  def test_require_extension
    assert !Rjb::loaded?
    $LOAD_PATH << '.'
    require 'rjbtest.jar'
    Rjb::load
    assert Rjb::loaded?
    base = jp.co.infoseek.hp.arton.rjb.Base.new
    assert_equal('hello', base.instance_var)
  end
end
