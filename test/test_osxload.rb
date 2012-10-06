# coding: utf-8

require 'rbconfig'
require 'test/unit'

class TestOSXLoad < Test::Unit::TestCase
  def setup
    ENV['JAVA_HOME'] = ''
    ENV['JVM_LIB'] = ''
    @testprog = File.dirname($0) + File::SEPARATOR + 'osx_jvmcheck.rb'
  end

  def test_no_java_home
    skip "no meaning test except for OSX" unless /darwin/ =~ RUBY_PLATFORM

    javahome = `/usr/libexec/java_home`
    if javahome =~ /jdk1\.[7-8]\.0/
      vendor = /Oracle/
      version = /1\.[7-8]\.0/
    else
      vendor = /Apple/
      version = /1\.[4-6]\.0/
    end
    test = `#{RbConfig.ruby} #{@testprog}`
    assert test =~ vendor, expected(vendor, test)
    assert test =~ version, expected(version, test)
  end

  def test_apple_jvm
    skip "no meaning test except for OSX" unless /darwin/ =~ RUBY_PLATFORM

    test_specific_jvm('/System/Library/Frameworks/JavaVM.framework/Home',
                      /Apple/)
  end

  def test_oracle_jvm
    skip "no meaning test except for OSX" unless /darwin/ =~ RUBY_PLATFORM

    test_specific_jvm('/Library/Java/JavaVirtualMachines/***/Contents/Home',
                      /Oracle/)
  end

  def test_withjvmlib
    skip "no meaning test except for OSX" unless /darwin/ =~ RUBY_PLATFORM

    ENV['JVM_LIB'] = '/usr/lib/libc.dylib'
    test = `#{RbConfig.ruby} #{@testprog}`.strip
    assert test == '', "no exception but #{test}"
  end

  private
  def test_specific_jvm(path, vendor)
    jvms = Dir.glob(path)
    skip "no #{vendor.inspect} jvm" if jvms.size == 0
    ENV['JAVA_HOME'] = jvms[0]
    test = `#{RbConfig.ruby} #{@testprog}`.strip
    assert test =~ vendor, expected(vendor, test)
  end

  def expected(test, target)
    "expected #{test.inspect} but #{target}"
  end
end

