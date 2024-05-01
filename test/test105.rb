begin
  require 'rjb'
rescue LoadError
  require 'rubygems'
  require 'rjb'
end
require 'test/unit'

class Test105 < Test::Unit::TestCase
  include Rjb
  def setup
    Rjb::load('.')
    @test105 = import('jp.co.infoseek.hp.arton.rjb.Test105')
  end

  def test_field
    test = @test105.new('xyz')
    assert_equal('xyz', test.test)
  end

  def test_method
    test = @test105.new('xyz')
    ret = test._invoke('test')
    assert_equal('method_xyz', ret)
  end
end

    
