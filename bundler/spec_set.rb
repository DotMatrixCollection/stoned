require "bundler"

module Bundler
  class SpecSet
    def initialize(specs = [])
      @specs = specs
    end

    def [](index)
      @specs[index]
    end

    def each(&block)
      @specs.each(&block)
    end

    def length
      @specs.length
    end

    def size
      length
    end

    def empty?
      @specs.empty?
    end

    def names
      @specs.map(&:name)
    end

    def for(dependencies)
      deps = Array(dependencies)
      matches = @specs.select do |spec|
        deps.any? { |dep| dep.name == spec.name }
      end
      Bundler::SpecSet.new(matches)
    end

    def find_all_by_name(name)
      @specs.select { |spec| spec.name == name.to_s }
    end

    def find_by_name_and_platform(name, _platform = nil)
      find_all_by_name(name)[0]
    end

    def materialized_for_all_platforms
      self
    end

    def sorted
      @specs.sort_by { |spec| [spec.name.to_s, Gem::Version.new(spec.version.to_s)] }
    end

    def to_hash
      hash = {}
      @specs.each { |spec| hash[spec.name] = spec }
      hash
    end

    def to_a
      @specs
    end
  end
end
