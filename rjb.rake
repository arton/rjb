require 'rubygems'
begin
  require 'rake/gempackagetask'
  $package_task = Rake::GemPackageTask
rescue
  require 'rubygems/package_task'
  $package_task = Gem::PackageTask
end
require 'fileutils'

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
  if /mswin|mingw|darwin/ =~ RUBY_PLATFORM
    s.platform = Gem::Platform::CURRENT
  else
    s.platform = Gem::Platform::RUBY
    s.extensions << 'ext/extconf.rb'
  end
  s.required_ruby_version = '>= 1.8.2'
  s.summary = 'Ruby Java bridge'
  s.name = 'rjb'
  s.homepage = 'http://rjb.rubyforge.org/'
  s.rubyforge_project = 'rjb'
  s.version = read_version
  s.requirements << 'none'
  s.require_path = 'lib'
  s.requirements << 'JDK 5.0'
  s.license = 'LGPL'
  files = FileList['ext/*.java', 'ext/*.c', 'ext/*.h', 'ext/depend',
                   'data/rjb/**/*.class', 'lib/*.rb', 'lib/rjb/*.rb', 'samples/**/*.rb', 
                   'test/*.rb', 'test/**/*.class', 'test/*.jar', 'COPYING', 'ChangeLog', 'readme.*']
  if /mswin|mingw/ =~ RUBY_PLATFORM
    FileUtils.cp 'ext/rjbcore.so', 'lib/rjbcore.so'
    files << "lib/rjbcore.so"
    s.requirements << ' VC6 version of Ruby' if RUBY_PLATFORM =~ /mswin/
  elsif /darwin/ =~ RUBY_PLATFORM
    FileUtils.cp 'ext/rjbcore.bundle', 'lib/rjbcore.bundle'
    files << "lib/rjbcore.bundle"
  end
  s.files = files
  s.test_file = 'test/test.rb'
  s.description = <<EOD
RJB is a bridge program that connect between Ruby and Java with Java Native Interface.
EOD
end

$package_task.new(spec) do |pkg|
  pkg.gem_spec = spec
  pkg.need_zip = false
  pkg.need_tar = false
end
