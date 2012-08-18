=begin
  Copyright(c) 2006-2010 arton
=end

require 'rbconfig'

module RjbConf
  dir = File.join(File.dirname(File.dirname(__FILE__)), 'data')
  if File.exist?(dir)
    datadir = dir
  else
    datadir = RbConfig::CONFIG['datadir']
  end
  BRIDGE_FILE = File.join(datadir, 'rjb', 'jp', 'co', 'infoseek', 'hp',
                          'arton', 'rjb', 'RBridge.class')
  unless File.exist?(BRIDGE_FILE)
    raise 'bridge file not found'
  end
end

require 'rjbcore'

module Rjb
  module MODIFIER
    def self.STATIC
      8
    end
    def self.PUBLIC
      1
    end
  end

  module JMethod
    def instance_method?(m)
      m.modifiers & MODIFIER.STATIC == 0
    end
    def public_method?(m)
      (m.modifiers & MODIFIER.PUBLIC) == MODIFIER.PUBLIC
    end
    def jmethods(org, klass, &blk)
      (org + klass.getMethods.select do |m|
         blk.call(m)
      end.map do |m|
        m.name
      end).uniq
    end
    def format_sigs(s)
      if s.size < 0
        ''
      elsif s.size == 1
        s[0]
      else
        "[#{s.map{|m|m.nil? ? 'void' : m}.join(', ')}]"
      end
    end
  end

  class Rjb_JavaClass
    include JMethod
    def public_methods(inh = true)
      jmethods(super(inh), self) do |m|
        !instance_method?(m) && public_method?(m)
      end
    end
    def methods(inh = true)
      jmethods(super(inh), self) do |m|
        !instance_method?(m) && public_method?(m)
      end
    end
    def java_methods
      jmethods([], self) do |m|
        !instance_method?(m) && public_method?(m)
      end.map do |m|
        "#{m}(#{format_sigs(self.static_sigs(m))})"
      end
    end
  end
  class Rjb_JavaProxy
    include JMethod
    def initialize_proxy
    end
    def public_methods(inh = true)
      jmethods(super(inh), getClass) do |m|
        instance_method?(m) && public_method?(m)
      end
    end
    def methods(inh = true)
      jmethods(super(inh), getClass) do |m|
        instance_method?(m) && public_method?(m)
      end
    end
    def java_methods
      jmethods([], getClass) do |m|
        instance_method?(m) && public_method?(m)
      end.map do |m|
        "#{m}(#{format_sigs(getClass.sigs(m))})"
      end
    end
  end
  class Rjb_JavaBridge
    def method_missing(name, *args)
      @wrapped.__send__(name, *args)
    end
    def respond_to?(name, inc_private = false)
      @wrapped.respond_to?(name, inc_private)
    end
    attr_reader :wrapped
  end
end
