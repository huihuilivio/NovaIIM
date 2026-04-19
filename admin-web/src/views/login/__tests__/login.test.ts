import { describe, it, expect } from 'vitest'
import { mount } from '@vue/test-utils'
import { createPinia } from 'pinia'
import LoginView from '@/views/login/LoginView.vue'
import ElementPlus from 'element-plus'

describe('LoginView', () => {
  it('应渲染登录表单', () => {
    const wrapper = mount(LoginView, {
      global: {
        plugins: [createPinia(), ElementPlus],
        stubs: { 'router-link': true, 'router-view': true },
        mocks: {
          $router: { push: () => {} },
          $route: { path: '/login' },
        },
      },
    })
    expect(wrapper.find('.login-card').exists()).toBe(true)
    expect(wrapper.find('h1').text()).toContain('NovaIIM')
  })
})
