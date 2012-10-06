# coding: utf-8
begin
  require 'rjb'
rescue LoadError 
  require 'rubygems' 
  require 'rjb'
end
S = Rjb::import('java.lang.System')
puts "#{S.property('java.vendor')} #{S.property('java.version')}"
