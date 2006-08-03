require 'test/unit'
require 'rjb'

class TestRjbGC < Test::Unit::TestCase
  include Rjb
  def setup
    load(nil, ['-verbose:gc'])
  end

  def tearDown
    unload
  end

  def test_gc
    stringBuffer = import('java.lang.StringBuffer')
    (0..1000).each do |i|
      sb = stringBuffer.new
      (0..1000).each do |j|
	sb.append('                                                        ')
      end
      GC.start
    end
  end
end
