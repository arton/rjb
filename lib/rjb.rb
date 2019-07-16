=begin
  Copyright(c) 2006-2010,2012 arton
=end

require 'rbconfig'

begin
  require 'rubinius/ffi'
  module DL
    extend FFI::Library
    class FakeDL
      def initialize(lib)
        @lib = lib
      end
      def [](fun)
        f = @lib.find_function(fun)
        if f
          f.to_i
        else
          nil
        end
      end
    end
    def self.dlopen(lib)
      a = ffi_lib(lib)
      if Array === a && a.size >= 1
        FakeDL.new(a[0])
      else
        nil
      end
    end
  end
rescue LoadError
end

module RjbConf
  if /darwin/ =~ RUBY_PLATFORM
    if ENV['JVM_LIB'].nil? || ENV['JVM_LIB'] == ''
      if ENV['JAVA_HOME'].nil? || ENV['JAVA_HOME'] == ''
        jvms = Dir.glob("#{`/usr/libexec/java_home`.strip}/**/libjvm.dylib")
      else
        jvms = Dir.glob("#{ENV['JAVA_HOME']}/**/libjvm.dylib")
      end
      if jvms.size > 0
        ENV['JVM_LIB'] = jvms[0]
      end
    end
  elsif /win32|win64/ =~ RUBY_PLATFORM
    # add JRE bin directory for further DLLs
    ENV['PATH'] = "#{ENV['JAVA_HOME']}\\jre\\bin;#{ENV['PATH']}"
  end

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
        m.name.to_sym
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
    def make_snake(nm)
      nm.gsub(/(.)([A-Z])/) { "#{$1}_#{$2.downcase}" }
    end
    alias :rjb_org_respond_to? :respond_to?
    def rjb_respond_to?(sym, klass, priv)
      return true if (klass ? self : getClass).getMethods.select do |m|
        (klass && !instance_method?(m) && (priv || public_method?(m))) ||
          (!klass && instance_method?(m) && (priv || public_method?(m)))
      end.map do |m|
        [m.name.to_sym, make_snake(m.name).to_sym]
      end.flatten.include?(sym.to_sym)
      rjb_org_respond_to?(sym, priv)
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
    def respond_to?(sym, priv = false)
      rjb_respond_to?(sym, true, priv)
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
    def respond_to?(sym, priv = false)
      rjb_respond_to?(sym, false, priv)
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
