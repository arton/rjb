require 'rubygems'
require 'rake/gempackagetask'

def read_version
  File.open('ext/rjb.c').each_line do |x|
    m = /RJB_VERSION\s+"(.+?)"/.match(x)
    if m
      return m[1]
    end
  end
  nil
end

desc "Default Task"
task :default => [ :package ]

spec = Gem::Specification.new do |s|
  s.authors = 'arton'
  s.email = 'artonx@gmail.com'
  if /mswin32|mingw/ =~ RUBY_PLATFORM
    s.platform = Gem::Platform::WIN32
  else
    s.platform = Gem::Platform::RUBY
    s.extensions << 'ext/extconf.rb'
  end
  s.required_ruby_version = '>= 1.8.2'
  s.summary = 'Ruby Java bridge'
  s.name = 'rjb'
  s.homepage = 'http://arton.no-ip.info/collabo/backyard/?RubyJavaBridge'
  s.version = read_version
  s.requirements << 'none'
  s.require_path = 'lib'
  s.requirements << 'JDK 5.0'
  files = FileList['ext/*.java', 'ext/*.c', 'ext/*.h', 'ext/depend',
                   'data/rjb/**/*.class', 'lib/*.rb', 'samples/**/*.rb', 
                   'test/*.rb', 'test/**/*.class', 'COPYING', 'ChangeLog', 'readme.*']
  if /mswin32/ =~ RUBY_PLATFORM
    files << "lib/rjbcore.so"
    s.requirements << ' VC6 version of Ruby' 
  end
  s.files = files
  s.test_file = 'test/test.rb'
  s.description = <<EOD
RJB is a bridge program that connect between Ruby and Java with Java Native Interface.
EOD
end

Rake::GemPackageTask.new(spec) do |pkg|
  pkg.gem_spec = spec
  pkg.need_zip = false
  pkg.need_tar = false
end
